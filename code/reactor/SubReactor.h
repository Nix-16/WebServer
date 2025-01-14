#ifndef SUBREACTOR_H
#define SUBREACTOR_H

#include <unordered_map>
#include <memory>
#include <mutex>        // 引入 mutex
#include <stdio.h>      // 标准输入输出
#include <stdlib.h>     // 标准库函数
#include <string.h>     // 字符串操作
#include <unistd.h>     // close()
#include <arpa/inet.h>  // htons(), inet_pton()
#include <sys/types.h>  // 基本数据类型
#include <sys/socket.h> // 套接字接口
#include <netinet/in.h> // sockaddr_in
#include "Epoll.h"
#include "HttpConn.h"
#include "ThreadPool.h"
#include "log.hpp"

class SubReactor
{
public:
    SubReactor(std::shared_ptr<ThreadPool> threadPool);
    ~SubReactor();

    // 启动 SubReactor 的事件循环
    void run();

    // 关闭 SubReactor 的事件循环
    void stop();

    // 将新的 clientFd 加入 epoll 监控
    void AddConn(int fd, const sockaddr_in &addr);

    // 关闭连接
    void CloseConn(int fd);

private:
    // 处理事件
    void HandleEvents_(int fd, uint32_t events);
    void HandleRead_(HttpConn &conn);
    void HandleWrite_(HttpConn &conn);

private:
    std::unique_ptr<Epoll> epoller_;
    // 该 SubReactor 只管理自己的一些客户端连接
    std::unordered_map<int, HttpConn> users_;
    // 保护 users_ 容器的互斥锁
    std::mutex users_mutex_;

    // 线程池
    std::shared_ptr<ThreadPool> threadPool_;

    // 你可以自行选择在构造时创建一个线程，也可以外部控制
    bool isRunning_;

    AsyncLogger *logger;
};

#endif // SUBREACTOR_H
