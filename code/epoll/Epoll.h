#ifndef EPOLL_H
#define EPOLL_H

#include <sys/epoll.h>
#include <unistd.h>
#include <vector>
#include <cerrno>
#include <stdexcept>

class Epoll
{
public:
    /**
     * @brief 构造函数：创建 epoll 实例并初始化事件数组
     * @param maxEvent epoll_wait 时可返回的最大事件数
     */
    explicit Epoll(int maxEvent = 1024);

    /**
     * @brief 析构函数：关闭 epoll 文件描述符
     */
    ~Epoll();

    /**
     * @brief 向 epoll 中添加一个文件描述符
     * @param fd    要添加的 fd
     * @param events 监听的事件，如 EPOLLIN | EPOLLOUT | EPOLLET | EPOLLONESHOT 等
     * @return 成功返回 0，失败返回 -1
     */
    int AddFd(int fd, uint32_t events);

    /**
     * @brief 修改已存在的 fd 的监听事件
     * @param fd    要修改的 fd
     * @param events 新的事件
     * @return 成功返回 0，失败返回 -1
     */
    int ModFd(int fd, uint32_t events);

    /**
     * @brief 从 epoll 中移除一个文件描述符
     * @param fd 要移除的 fd
     * @return 成功返回 0，失败返回 -1
     */
    int DelFd(int fd);

    /**
     * @brief 等待事件触发
     * @param timeoutMS 超时时间（毫秒），-1 表示阻塞等待
     * @return 返回就绪事件的数量(>=0)，出错返回 -1
     */
    int Wait(int timeoutMS = -1);

    /**
     * @brief 获取第 i 个就绪事件对应的 fd
     * @param i 就绪事件在数组中的索引
     * @return 对应的 fd
     */
    int GetEventFd(size_t i) const;

    /**
     * @brief 获取第 i 个就绪事件的类型(EPOLLIN / EPOLLOUT / EPOLLERR / ...)
     * @param i 就绪事件在数组中的索引
     * @return 事件类型
     */
    uint32_t GetEvents(size_t i) const;

private:
    int epollFd_;                     // epoll 文件描述符
    std::vector<epoll_event> events_; // 就绪事件列表
};

#endif // EPOLL_H
