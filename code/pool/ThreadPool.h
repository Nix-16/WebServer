#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include "log.hpp"

/**
 * @brief 线程池，用于高并发服务器处理网络 I/O 或其他耗时任务
 */
class ThreadPool
{
public:
    /**
     * @brief 构造函数：创建固定数量的线程，启动它们等待任务
     * @param threadCount 线程数量
     */
    explicit ThreadPool(size_t threadCount = 8);

    /**
     * @brief 析构函数：通知所有线程停止，并等待它们执行完后再退出
     */
    ~ThreadPool();

    /**
     * @brief 向线程池提交一个任务
     * @param task 可以是任何可调用对象，例如 `lambda`, `std::bind`, 仿函数等
     */
    void addTask(std::function<void()> task);

private:
    /**
     * @brief 工作线程函数：不停从任务队列里取任务执行，直到收到结束信号
     */
    void workerThread();

private:
    std::vector<std::thread> workers_;        // 工作线程数组
    std::queue<std::function<void()>> tasks_; // 任务队列

    std::mutex mtx_;               // 互斥锁，保护 tasks_ 队列
    std::condition_variable cond_; // 条件变量，用于唤醒工作线程
    std::atomic<bool> stop_;       // 用于通知线程池停止

    AsyncLogger *logger;
};

#endif // THREADPOOL_H
