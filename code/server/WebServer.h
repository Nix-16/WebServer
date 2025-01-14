#ifndef SERVER_H
#define SERVER_H

#include "MasterReactor.h"
#include "config.h"
#include "log.hpp"

/**
 * @brief Server 类，用来包装 MasterReactor（以及子 Reactor 的管理）
 *        实际上 MasterReactor 内部已经做了主 Reactor + 多个 SubReactor
 */
class Server
{
public:
    /**
     * @brief 构造函数
     * @param port 监听端口
     * @param subReactorCount 子 Reactor 个数
     */
    Server(int port, int subReactorCount = 4);

    ~Server();

    /**
     * @brief 启动服务器(实际上调用 master_.run())
     */
    void start();

    /**
     * @brief 停止服务器(调用 master_.stop())
     */
    void stop();

private:
    MasterReactor master_; ///< 内部持有一个 MasterReactor

    bool running_;

    AsyncLogger* logger; 

    Config *config;
};

#endif // SERVER_H
