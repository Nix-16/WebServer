#ifndef MASTERREACTOR_H
#define MASTERREACTOR_H

#include <memory>
#include <vector>
#include <thread>
#include <atomic>
#include <netinet/in.h>
#include <unistd.h> // close
#include <fcntl.h>  // fcntl
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>

#include "Epoll.h"
#include "SubReactor.h"
#include "config.h"
#include "HttpConn.h"

/**
 * @brief 主 Reactor，监听 listenFd，接受新连接，并分配给多条 SubReactor
 */
class MasterReactor
{
public:
    /**
     * @param port           监听端口
     * @param subReactorCnt  创建多少个 SubReactor
     */
    MasterReactor(int port, int subReactorCnt = 4);
    ~MasterReactor();

    /**
     * @brief 启动事件循环
     */
    void run();

    /**
     * @brief 停止主 Reactor
     */
    void stop();

private:
    void InitSocket_();
    void HandleListen_();

private:
    int port_;
    int listenFd_;
    bool isRunning_;

    std::unique_ptr<Epoll> epoller_;

    std::vector<std::unique_ptr<SubReactor>> subReactors_; // 多个子 Reactor
    std::vector<std::thread> subThreads_;                  // 子 Reactor 对应的线程
    std::shared_ptr<ThreadPool> threadPool_;               // 线程池

    AsyncLogger *logger;

    Config *config;
};

#endif // MASTERREACTOR_H
