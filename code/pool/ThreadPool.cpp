#include "ThreadPool.h"
#include <iostream>

ThreadPool::ThreadPool(size_t threadCount)
    : stop_(false)
{
    // 创建 threadCount 条线程
    for (size_t i = 0; i < threadCount; i++)
    {
        workers_.emplace_back(std::thread(&ThreadPool::workerThread, this));
    }
    std::cout<<"init threadpool num : "<<threadCount<<std::endl;
}

ThreadPool::~ThreadPool()
{
    // 通知所有线程该停止了
    stop_.store(true, std::memory_order_release);

    // 唤醒所有等待的线程
    cond_.notify_all();

    // 等待所有线程结束
    for (auto &th : workers_)
    {
        if (th.joinable())
        {
            th.join();
        }
    }
}

void ThreadPool::addTask(std::function<void()> task)
{
    {
        std::unique_lock<std::mutex> locker(mtx_);
        // 将任务压入队列
        tasks_.push(task);
    }
    // 唤醒一个工作线程
    cond_.notify_one();
}

void ThreadPool::workerThread()
{
    // 工作线程函数：不断从任务队列里取任务并执行
    while (true)
    {
        std::function<void()> task;
        {
            // 获取锁
            std::unique_lock<std::mutex> locker(mtx_);
            // 等待条件：队列不为空 或 收到停止信号
            cond_.wait(locker, [this]
                       { return !tasks_.empty() || stop_.load(std::memory_order_acquire); });

            // 如果停止并且队列空了，就退出线程
            if (stop_.load(std::memory_order_acquire) && tasks_.empty())
            {
                return;
            }

            // 从队列取出一个任务
            task = tasks_.front();
            tasks_.pop();
        }
        // 解锁后执行任务
        try
        {
            task();
        }
        catch (...)
        {
            // 如果任务抛出异常，这里可以选择处理或忽略
            std::cerr << "[ThreadPool] Task threw an exception!\n";
        }
    }
}
