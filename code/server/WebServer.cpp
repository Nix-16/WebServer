#include "WebServer.h"

Server::Server(int port, int subReactorCount)
    : master_(port, subReactorCount),
      running_(false),
      logger(&AsyncLogger::get_instance()),
      config(&Config::GetInstance()) // 获取配置的单例实例
{

    // 初始化数据库连接池
    // SqlConnPool::Instance()->Init("localhost", 3306, "root", "6", "webserver", 4);
    SqlConnPool::Instance()->Init(config->GetDBHost().c_str(), config->GetDBPort(), config->GetDBUser().c_str(), config->GetDBPassword().c_str(), config->GetDBName().c_str(), config->GetSqlPoolNum());
}

Server::~Server()
{
    stop();
}

void Server::start()
{
    running_ = true;

    std::cout << "MasterReactor is runing----" << std::endl;
    // 让 MasterReactor 开始工作(内部会启动 subReactor 线程)
    master_.run(); // 阻塞或循环
}

void Server::stop()
{
    if (!running_)
        return;
    running_ = false;
    // 通知 MasterReactor 停止(顺便子 Reactor 也会停)
    master_.stop();
    logger->log(INFO, "MasterReactor is stop");
}
