#include "Epoll.h"
#include <stdexcept>

/**
 * @brief 构造函数
 * 使用 epoll_create1(0) 创建 epollFd_。如果不支持，可使用 epoll_create()
 */
Epoll::Epoll(int maxEvent)
    : epollFd_(-1),
      events_(maxEvent)
{
    // 创建 epoll fd
#ifdef __linux__
    // 推荐使用 epoll_create1 代替 epoll_create
    epollFd_ = epoll_create1(0);
#else
    epollFd_ = epoll_create(maxEvent);
#endif
    if (epollFd_ < 0)
    {
        throw std::runtime_error("epoll create failed!");
    }
}

/**
 * @brief 析构函数：关闭 epollFd_
 */
Epoll::~Epoll()
{
    if (epollFd_ >= 0)
    {
        close(epollFd_);
    }
}

int Epoll::AddFd(int fd, uint32_t events)
{
    if (fd < 0)
        return -1;

    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    // 添加到 epoll
    return epoll_ctl(epollFd_, EPOLL_CTL_ADD, fd, &ev);
}

int Epoll::ModFd(int fd, uint32_t events)
{
    if (fd < 0)
        return -1;

    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = events;

    return epoll_ctl(epollFd_, EPOLL_CTL_MOD, fd, &ev);
}

int Epoll::DelFd(int fd)
{
    if (fd < 0)
        return -1;

    epoll_event ev{}; // 这个参数不一定需要，但有些实现里要传
    return epoll_ctl(epollFd_, EPOLL_CTL_DEL, fd, &ev);
}

/**
 * @brief 等待事件触发
 * @param timeoutMS 超时时间(毫秒)，-1 表示永久阻塞
 * @return 就绪事件的数量，<0 表示出错
 */
int Epoll::Wait(int timeoutMS)
{
    int nReady = epoll_wait(epollFd_, events_.data(), static_cast<int>(events_.size()), timeoutMS);
    return nReady;
}

int Epoll::GetEventFd(size_t i) const
{
    return events_[i].data.fd;
}

uint32_t Epoll::GetEvents(size_t i) const
{
    return events_[i].events;
}
