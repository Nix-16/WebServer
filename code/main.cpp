#include "./server/WebServer.h"

int main()
{
    // 获取配置的单例实例
    Config &config = Config::GetInstance();

    // 加载配置文件
    if (!config.LoadConfig("../config.json"))
    {
        return 0;
    }

    Server server(config.GetServerPort(), config.GetSubReactorNum());
    server.start(); // 进入主循环
    return 0;
}
