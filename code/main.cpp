#include "./server/WebServer.h"
#include "./log/log.hpp"

int main()
{
    // 获取配置的单例实例
    Config &config = Config::GetInstance();

    AsyncLogger &logger = AsyncLogger::get_instance();
    logger.set_log_file("async_log.log");

    logger.start(); // 启动日志线程

    // 加载配置文件
    if (!config.LoadConfig("../config.json"))
    {
        logger.log(ERROR, "Load config error!");
        return 0;
    }

    Server server(config.GetServerPort(), config.GetSubReactorNum());
    server.start(); // 进入主循环

    logger.stop();
    return 0;
}
