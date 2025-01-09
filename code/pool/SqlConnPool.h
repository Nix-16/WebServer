#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <atomic>
#include <thread>
#include <iostream>

/**
 * @brief 数据库连接池，用于管理 MySQL 连接的创建、获取、归还与销毁。
 *        采用单例模式，支持多线程安全、超时获取连接等功能。
 */
class SqlConnPool
{
public:
    /**
     * @brief 获取连接池单例实例
     */
    static SqlConnPool *Instance();

    /**
     * @brief 初始化连接池
     * @param host     数据库主机
     * @param port     数据库端口
     * @param user     数据库用户名
     * @param pwd      数据库密码
     * @param dbName   数据库名称
     * @param connSize 连接池中预创建的连接数量
     */
    void Init(const char *host, int port,
              const char *user, const char *pwd,
              const char *dbName, int connSize);

    /**
     * @brief 获取一个数据库连接
     * @param timeout_ms 超时时间（毫秒），默认 1000ms。若超时则返回 nullptr。
     * @return MYSQL* 指针；若获取失败或超时，则返回 nullptr。
     */
    MYSQL *GetConn(int timeout_ms = 1000);

    /**
     * @brief 归还一个数据库连接到连接池
     */
    void FreeConn(MYSQL *conn);

    /**
     * @brief 获取当前空闲连接数量
     */
    int GetFreeConnCount() const;

    /**
     * @brief 关闭连接池，释放所有连接
     */
    void ClosePool();

    /**
     * @brief 析构函数，自动关闭连接池
     */
    ~SqlConnPool();

private:
    /**
     * @brief 私有构造，单例模式
     */
    SqlConnPool();

    /**
     * @brief 创建一个新的 MySQL 连接
     */
    MYSQL *createConn(const char *host, int port,
                      const char *user, const char *pwd,
                      const char *dbName);

    /**
     * @brief 关闭并释放一个 MySQL 连接
     */
    void closeConn(MYSQL *conn);

private:
    int MAX_CONN_;  // 最大连接数
    int useCount_;  // 当前已被使用的连接数
    int freeCount_; // 当前空闲连接数

    std::queue<MYSQL *> connQue_; // 存放空闲连接的队列
    mutable std::mutex mtx_;      // 保护 connQue_ 的线程安全
    sem_t semId_;                 // 信号量，控制可用连接的数量
};

#endif // SQLCONNPOOL_H
