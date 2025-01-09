#include "SqlConnPool.h"
#include <chrono>
#include <cerrno>
#include <ctime>

/**
 * @brief 静态单例指针：通过函数内 static 实现
 */
SqlConnPool *SqlConnPool::Instance()
{
    static SqlConnPool pool;
    return &pool;
}

/**
 * @brief 私有构造：初始化计数和信号量
 */
SqlConnPool::SqlConnPool()
    : MAX_CONN_(0),
      useCount_(0),
      freeCount_(0)
{
    // 初始时没有连接，信号量值设为0
    sem_init(&semId_, 0, 0);
}

/**
 * @brief 析构时自动关闭连接池
 */
SqlConnPool::~SqlConnPool()
{
    ClosePool();
}

/**
 * @brief 初始化连接池，创建 connSize 条 MySQL 连接
 */
void SqlConnPool::Init(const char *host, int port,
                       const char *user, const char *pwd,
                       const char *dbName, int connSize)
{
    std::lock_guard<std::mutex> locker(mtx_);
    if (!connQue_.empty())
    {
        std::cerr << "[SqlConnPool] Already initialized.\n";
        return;
    }

    MAX_CONN_ = connSize;
    for (int i = 0; i < connSize; i++)
    {
        MYSQL *conn = createConn(host, port, user, pwd, dbName);
        if (!conn)
        {
            std::cerr << "[SqlConnPool] Create MySQL connection failed!\n";
            continue;
        }
        connQue_.push(conn);
    }
    // 实际成功创建的连接数
    freeCount_ = connQue_.size();

    // 重新初始化信号量，值为 freeCount_
    sem_destroy(&semId_);
    sem_init(&semId_, 0, freeCount_);

    std::cout << "[SqlConnPool] Init done. PoolSize = "
              << freeCount_ << "/" << MAX_CONN_ << "\n";
}

/**
 * @brief 获取一个可用连接，带超时功能
 * @param timeout_ms 默认1秒，超时后返回 nullptr
 */
MYSQL *SqlConnPool::GetConn(int timeout_ms)
{
    MYSQL *conn = nullptr;

    if (connQue_.empty())
    {
        std::cerr << "[SqlConnPool] No available connection.\n";
        return nullptr;
    }

    // 带超时的信号量等待
    if (timeout_ms <= 0)
    {
        // 不指定超时则阻塞等待
        sem_wait(&semId_);
    }
    else
    {
        // 计算超时时刻
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        long nsec = ts.tv_nsec + (timeout_ms % 1000) * 1000000L;
        ts.tv_sec += timeout_ms / 1000 + nsec / 1000000000L;
        ts.tv_nsec = nsec % 1000000000L;

        if (sem_timedwait(&semId_, &ts) == -1)
        {
            if (errno == ETIMEDOUT)
            {
                std::cerr << "[SqlConnPool] GetConn timeout("
                          << timeout_ms << " ms).\n";
                return nullptr;
            }
        }
    }

    // 成功拿到信号量
    {
        std::lock_guard<std::mutex> locker(mtx_);
        if (connQue_.empty())
        {
            // 理论上不该出现，但多一层保护
            std::cerr << "[SqlConnPool] GetConn - queue empty.\n";
            // sem_post(&semId_); // 视情况而定
            return nullptr;
        }
        conn = connQue_.front();
        connQue_.pop();
        freeCount_--;
        useCount_++;
    }

    return conn;
}

/**
 * @brief 归还连接到连接池
 */
void SqlConnPool::FreeConn(MYSQL *conn)
{
    if (!conn)
        return;
    {
        std::lock_guard<std::mutex> locker(mtx_);
        connQue_.push(conn);
        freeCount_++;
        useCount_--;
    }
    // 通知有一个可用连接
    sem_post(&semId_);
}

/**
 * @brief 获取当前空闲连接数量
 */
int SqlConnPool::GetFreeConnCount() const
{
    std::lock_guard<std::mutex> locker(mtx_);
    return freeCount_;
}

/**
 * @brief 关闭连接池，释放所有连接
 */
void SqlConnPool::ClosePool()
{
    std::lock_guard<std::mutex> locker(mtx_);
    while (!connQue_.empty())
    {
        MYSQL *conn = connQue_.front();
        connQue_.pop();
        closeConn(conn);
    }
    freeCount_ = 0;
    useCount_ = 0;
    sem_destroy(&semId_);
    std::cout << "[SqlConnPool] Pool closed.\n";
}

/* ================== 私有工具函数 =================== */

/**
 * @brief 创建一个新的 MySQL 连接
 */
MYSQL *SqlConnPool::createConn(const char *host, int port,
                               const char *user, const char *pwd,
                               const char *dbName)
{
    MYSQL *conn = mysql_init(nullptr);
    if (!conn)
    {
        std::cerr << "[SqlConnPool] mysql_init() failed.\n";
        return nullptr;
    }
    // 若需要设置字符集等，可在此添加
    // mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8");

    conn = mysql_real_connect(conn, host, user, pwd, dbName,
                              port, nullptr, 0);
    if (!conn)
    {
        std::cerr << "[SqlConnPool] mysql_real_connect() error: "
                  << mysql_error(conn) << "\n";
        return nullptr;
    }
    return conn;
}

/**
 * @brief 关闭并释放一个 MySQL 连接
 */
void SqlConnPool::closeConn(MYSQL *conn)
{
    if (conn)
    {
        mysql_close(conn);
        conn = nullptr;
    }
}
