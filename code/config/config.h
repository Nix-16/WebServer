#ifndef CONFIG_H
#define CONFIG_H
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;

class Config
{
public:
    // 获取配置的单例实例
    static Config &GetInstance()
    {
        static Config instance; // 唯一实例
        return instance;
    }

    // 加载配置文件
    bool LoadConfig(const std::string &filepath)
    {
        // 如果已经加载过配置文件，直接返回
        if (isLoaded_)
        {
            std::cerr << "Config already loaded!" << std::endl;
            return true;
        }

        std::ifstream file(filepath);
        if (!file.is_open())
        {
            std::cerr << "Failed to open config file!" << std::endl;
            return false;
        }

        try
        {
            file >> config_;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error parsing config file: " << e.what() << std::endl;
            return false;
        }

        isLoaded_ = true;
        return true;
    }

    // 获取配置项
    std::string GetServerHost() const
    {
        return GetStringValue(config_, "server", "host", "127.0.0.1");
    }

    int GetServerPort() const
    {
        return GetIntValue(config_, "server", "port", 8080);
    }

    int GetSubReactorNum() const
    {
        return GetIntValue(config_, "server", "subReactorNum", 4);
    }

    std::string GetServerSrcDir() const
    {
        return GetStringValue(config_, "server", "srcDir", "../resources");
    }

    std::string GetDBHost() const
    {
        return GetStringValue(config_, "database", "host", "localhost");
    }

    int GetDBPort() const
    {
        return GetIntValue(config_, "database", "port", 3306);
    }

    std::string GetDBUser() const
    {
        return GetStringValue(config_, "database", "user", "root");
    }

    std::string GetDBPassword() const
    {
        return GetStringValue(config_, "database", "password", "password");
    }

    int GetSqlPoolNum() const
    {
        return GetIntValue(config_, "pool", "sqlPoolNum", 4);
    }

    std::string GetDBName() const
    {
        return GetStringValue(config_, "database", "dbname", "webserver");
    }

    int GetThreadPoolNum() const
    {
        return GetIntValue(config_, "pool", "threadPoolNum", 8);
    }

private:
    json config_;           // 存储配置
    bool isLoaded_ = false; // 配置是否已经加载

    // 获取字符串值，默认值为"default_value"
    std::string GetStringValue(const json &config, const std::string &section, const std::string &key, const std::string &default_value) const
    {
        try
        {
            return config.at(section).at(key).get<std::string>();
        }
        catch (const std::exception &)
        {
            std::cerr << "Warning: Missing or invalid key [" << section << "][" << key << "], using default: " << default_value << std::endl;
            return default_value;
        }
    }

    // 获取整数值，默认值为"default_value"
    int GetIntValue(const json &config, const std::string &section, const std::string &key, int default_value) const
    {
        try
        {
            return config.at(section).at(key).get<int>();
        }
        catch (const std::exception &)
        {
            std::cerr << "Warning: Missing or invalid key [" << section << "][" << key << "], using default: " << default_value << std::endl;
            return default_value;
        }
    }

    // 禁止拷贝和赋值
    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

    // 构造函数是私有的，确保不能从外部创建实例
    Config() {}
};
#endif

// int main()
// {
//     // 获取配置的单例实例
//     Config &config = Config::GetInstance();

//     // 加载配置文件
//     if (!config.LoadConfig("../../config.json"))
//     {
//         return -1;
//     }

//     // 获取配置项并输出
//     std::cout << "Server Host: " << config.GetServerHost() << std::endl;
//     std::cout << "Server Port: " << config.GetServerPort() << std::endl;
//     std::cout << "Sub Reactor Num: " << config.GetSubReactorNum() << std::endl;
//     std::cout << "Source Directory: " << config.GetServerSrcDir() << std::endl;
//     std::cout << "Database Host: " << config.GetDBHost() << std::endl;
//     std::cout << "Database Port: " << config.GetDBPort() << std::endl;
//     std::cout << "Database User: " << config.GetDBUser() << std::endl;
//     std::cout << "Database Password: " << config.GetDBPassword() << std::endl;
//     std::cout << "SQL Pool Num: " << config.GetSqlPoolNum() << std::endl;
//     std::cout << "Thread Pool Num: " << config.GetThreadPoolNum() << std::endl;

//     return 0;
// }
