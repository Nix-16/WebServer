#include "SubReactor.h"
#include <fcntl.h> // fcntl()
#include <errno.h>
#include <iostream>

SubReactor::SubReactor(std::shared_ptr<ThreadPool> threadPool)
    : epoller_(std::make_unique<Epoll>()),
      threadPool_(threadPool),
      isRunning_(false),
      logger(&AsyncLogger::get_instance())
{
}

SubReactor::~SubReactor()
{
    isRunning_ = false;
    // 在这里可做一些资源清理，如关闭所有连接
    std::lock_guard<std::mutex> lock(users_mutex_);
    for (auto &pair : users_)
    {
        pair.second.Close();
    }
}

// 启动子 Reactor 的事件循环
// 通常会在一个独立的线程里调用此函数
void SubReactor::run()
{
    isRunning_ = true;
    while (isRunning_)
    {
        // 等待就绪事件，可自行调节超时时间，如 1000ms
        int eventCount = epoller_->Wait(1000);
        if (eventCount < 0)
        {
            if (errno == EINTR)
                continue; // 被信号打断则继续
            std::cerr << "SubReactor epoll_wait error\n";
            logger->log(ERROR, "SubReactor epoll_wait error");
            break;
        }

        // 处理每一个就绪事件
        for (int i = 0; i < eventCount; ++i)
        {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);

            // 交给内部函数处理
            HandleEvents_(fd, events);
        }
    }
}

// 新连接加入 SubReactor 管理
void SubReactor::AddConn(int fd, const sockaddr_in &addr)
{
    // 锁住 users_，保证线程安全
    std::lock_guard<std::mutex> lock(users_mutex_);
    // 初始化连接
    users_[fd].init(fd, addr);

    // 设置非阻塞
    int oldFlag = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, oldFlag | O_NONBLOCK);

    // 将 fd 加入 epoll 监控，监听可读事件(EPOLLIN)、边缘触发(EPOLLET)，可选 EPOLLONESHOT
    // epoller_->AddFd(fd, EPOLLIN | EPOLLET);

    // 也可根据需要添加 EPOLLONESHOT:
    epoller_->AddFd(fd, EPOLLIN | EPOLLET | EPOLLONESHOT);
}

// 关闭连接
void SubReactor::CloseConn(int fd)
{
    // 锁住 users_，保证线程安全
    std::lock_guard<std::mutex> lock(users_mutex_);
    auto it = users_.find(fd);
    if (it == users_.end())
    {
        return; // 不存在
    }
    epoller_->DelFd(fd); // 从 epoll 移除
    it->second.Close();  // 关闭 socket
    users_.erase(fd);    // 从 map 中删除
}

// 内部函数：处理单个 fd 的事件
void SubReactor::HandleEvents_(int fd, uint32_t events)
{
    // 锁住 users_，保证线程安全
    std::lock_guard<std::mutex> lock(users_mutex_);
    // 先找到对应连接
    auto it = users_.find(fd);
    if (it == users_.end())
    {
        return;
    }

    // 引用 or 拷贝
    HttpConn &conn = it->second;

    // 如果是可读事件
    if (events & EPOLLIN)
    {
        // 把真正的读逻辑投递给线程池
        threadPool_->addTask([this, fd]()
                             {
            // 二次查找：确保在任务执行时，连接还在
            // （若你能保证连接一定在，就不用二次查找）
            auto iter = users_.find(fd);
            if(iter == users_.end()) return;

            HttpConn &connRef = iter->second;
            HandleRead_(connRef); });
    }
    else if (events & EPOLLOUT)
    {
        // 同理，写事件也丢给线程池
        threadPool_->addTask([this, fd]()
                             {
            auto iter = users_.find(fd);
            if(iter == users_.end()) return;

            HttpConn &connRef = iter->second;
            HandleWrite_(connRef); });
    }
    else
    {
        CloseConn(fd);
    }
}

// 处理读事件
void SubReactor::HandleRead_(HttpConn &conn)
{
    int err = 0;
    int ret = -1;
    ret = conn.read(&err);

    if (ret <= 0 && err != EAGAIN)
    {
        CloseConn(conn.GetFd());
    }
    // 准备写响应
    if (conn.process())
    {
        epoller_->ModFd(conn.GetFd(), EPOLLOUT | EPOLLET | EPOLLONESHOT);
    }
    else
    {
        epoller_->ModFd(conn.GetFd(), EPOLLIN | EPOLLET | EPOLLONESHOT);
    }
}

// 处理写事件
void SubReactor::HandleWrite_(HttpConn &conn)
{
    int err = 0;
    int ret = conn.write(&err);

    // 写缓冲已写完
    if (conn.ToWriteBytes() == 0)
    {
        // 判断是否保持长连接
        if (conn.IsKeepAlive())
        {
            epoller_->ModFd(conn.GetFd(), EPOLLIN | EPOLLET | EPOLLONESHOT);
            return;
        }
        else
        {
            CloseConn(conn.GetFd());
        }
        return;
    }

    // 写入未完成，缓冲区满
    if (err == EAGAIN || err == EWOULDBLOCK)
    {
        epoller_->ModFd(conn.GetFd(), EPOLLOUT | EPOLLET | EPOLLONESHOT);
        return;
    }

    CloseConn(conn.GetFd());
}

void SubReactor::stop()
{
    isRunning_ = false;
}