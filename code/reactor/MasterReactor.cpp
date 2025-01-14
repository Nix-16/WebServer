#include "MasterReactor.h"

MasterReactor::MasterReactor(int port, int subReactorCnt)
    : port_(port),
      listenFd_(-1),
      isRunning_(false),
      epoller_(std::make_unique<Epoll>()),
      logger(&AsyncLogger::get_instance()),
      config(&Config::GetInstance()) // 获取配置的单例实例
{
    // 1. 初始化监听套接字
    InitSocket_();

    // 初始化线程池
    // threadPool_ = std::make_shared<ThreadPool>(8);
    threadPool_ = std::make_shared<ThreadPool>(config->GetThreadPoolNum());

    // 2. 创建多个 SubReactor
    subReactors_.reserve(subReactorCnt);

    for (int i = 0; i < subReactorCnt; i++)
    {
        subReactors_.emplace_back(std::make_unique<SubReactor>(threadPool_));
    }
}

MasterReactor::~MasterReactor()
{
    // 停止并回收资源
    stop();
    if (listenFd_ >= 0)
    {
        close(listenFd_);
    }
}

void MasterReactor::run()
{
    // 启动 SubReactor：每个都在单独线程里执行 run()
    isRunning_ = true;
    for (auto &sub : subReactors_)
    {
        subThreads_.emplace_back([&sub]()
                                 {
                                     sub->run(); // 每个子 Reactor 自己 epoll_wait()
                                 });
    }

    // 主线程在此不断 epoll_wait，处理新连接
    while (isRunning_)
    {
        int eventCount = epoller_->Wait(0);
        if (eventCount < 0)
        {
            if (errno == EINTR)
            {
                continue; // 信号打断则重试
            }
            std::cerr << "epoll_wait error\n";
            logger->log(ERROR, "epoll_wait error!");
            break;
        }

        for (int i = 0; i < eventCount; i++)
        {
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);

            if (fd == listenFd_ && (events & EPOLLIN))
            {
                HandleListen_(); // 处理新连接
            }
            // MasterReactor 只处理 listenFd，其他 fd 不在这里管
        }
    }

    // 若跳出循环表示 isRunning_ = false 或出错
    // 在 stop() 里还会回收 SubReactor 线程
}

void MasterReactor::stop()
{
    // 如果已经在停止，就不重复
    if (!isRunning_)
        return;
    isRunning_ = false;

    // 让每个 SubReactor 也停下
    for (auto &sub : subReactors_)
    {
        sub->stop(); // 让子 Reactor 退出 run()
    }

    // 回收子线程
    for (auto &th : subThreads_)
    {
        if (th.joinable())
        {
            th.join();
        }
    }
    subThreads_.clear();
}

void MasterReactor::InitSocket_()
{
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0)
    {
        std::cerr << "Create socket error!\n";
        logger->log(ERROR, "Create socket error!");
        exit(EXIT_FAILURE);
    }

    // 端口复用
    int optval = 1;
    setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    // 绑定地址
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    std::cout << "port: " << port_ << std::endl;

    if (bind(listenFd_, (sockaddr *)&addr, sizeof(addr)) < 0)
    {
        std::cerr << "Bind error!\n";
        logger->log(ERROR, "Bind error!");
        exit(EXIT_FAILURE);
    }

    if (listen(listenFd_, 1024) < 0)
    {
        std::cerr << "Listen error!\n";
        logger->log(ERROR, "Listen error!");
        exit(EXIT_FAILURE);
    }

    epoller_->AddFd(listenFd_, EPOLLIN | EPOLLET);
    // 设置非阻塞
    fcntl(listenFd_, F_SETFL, fcntl(listenFd_, F_GETFL) | O_NONBLOCK);

    std::cout << "[MasterReactor] Listen at port " << port_ << "\n";
    logger->log(INFO, "webserver runing port: " + std::to_string(port_));
}

void MasterReactor::HandleListen_()
{
    sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);

    static int idx = 0; // 静态局部变量，用于轮询分配
    while (true)
    {
        int clientFd = accept(listenFd_, (sockaddr *)&clientAddr, &len);
        if (clientFd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 暂时没新连接了
                break;
            }
            else
            {
                std::cerr << "Accept error!\n";
                logger->log(ERROR, "Accept error!");
                break;
            }
        }

        // 轮询
        idx = (idx + 1) % subReactors_.size();
        // 分派给 subReactors_[idx]
        subReactors_[idx]->AddConn(clientFd, clientAddr);
    }
}
