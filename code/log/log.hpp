#ifndef ASYNC_LOGGER_HPP
#define ASYNC_LOGGER_HPP

#include <iostream>
#include <fstream>
#include <queue>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <ctime>
#include <sstream>

// 日志级别
enum LogLevel
{
    INFO,
    WARNING,
    ERROR,
    DEBUG
};

// 异步日志类
class AsyncLogger
{
public:
    // 获取单例实例
    static AsyncLogger &get_instance()
    {
        static AsyncLogger instance;
        return instance;
    }

    // 设置日志文件
    void set_log_file(const std::string &filename)
    {
        std::lock_guard<std::mutex> lock(file_mutex);
        if (log_file.is_open())
        {
            log_file.close();
        }
        log_file.open(filename, std::ios::app);
        if (!log_file)
        {
            throw std::runtime_error("Failed to open log file: " + filename);
        }
    }

    // 设置日志级别
    void set_log_level(LogLevel level)
    {
        log_level = level;
    }

    // 记录日志
    void log(LogLevel level, const std::string &message)
    {
        if (level < log_level)
            return;

        std::ostringstream log_stream;
        log_stream << "[" << get_current_time() << "] [" << level_to_string(level) << "] " << message;

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            log_queue.push(log_stream.str());
        }
        cv.notify_one(); // 通知日志线程
    }

    // 启动日志线程
    void start()
    {
        running = true;
        log_thread = std::thread(&AsyncLogger::process_logs, this);
    }

    // 停止日志线程
    void stop()
    {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            running = false;
        }
        cv.notify_one();
        if (log_thread.joinable())
        {
            log_thread.join();
        }
    }

private:
    AsyncLogger() : running(false), log_level(INFO) {}
    ~AsyncLogger() { stop(); }

    AsyncLogger(const AsyncLogger &) = delete;
    AsyncLogger &operator=(const AsyncLogger &) = delete;

    // 获取当前时间
    std::string get_current_time() const
    {
        std::time_t now = std::time(nullptr);
        char buffer[20];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        return std::string(buffer);
    }

    // 转换日志级别为字符串
    std::string level_to_string(LogLevel level) const
    {
        switch (level)
        {
        case INFO:
            return "INFO";
        case WARNING:
            return "WARNING";
        case ERROR:
            return "ERROR";
        case DEBUG:
            return "DEBUG";
        default:
            return "UNKNOWN";
        }
    }

    // 处理日志消息
    void process_logs()
    {
        while (running || !log_queue.empty())
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            cv.wait(lock, [this]()
                    { return !running || !log_queue.empty(); });

            while (!log_queue.empty())
            {
                std::string log_entry = log_queue.front();
                log_queue.pop();
                lock.unlock(); // 解锁队列以便主线程可以继续写入

                {
                    std::lock_guard<std::mutex> file_lock(file_mutex);
                    if (log_file.is_open())
                    {
                        log_file << log_entry << std::endl;
                    }
                    else
                    {
                        std::cerr << log_entry << std::endl;
                    }
                }

                lock.lock(); // 重新加锁以处理下一个日志消息
            }
        }
    }

    std::ofstream log_file;            // 日志文件
    std::queue<std::string> log_queue; // 日志队列
    std::mutex queue_mutex;            // 队列互斥锁
    std::mutex file_mutex;             // 文件互斥锁
    std::condition_variable cv;        // 条件变量
    std::thread log_thread;            // 日志线程
    std::atomic<bool> running;         // 运行标志
    LogLevel log_level;                // 日志级别
};

#endif // ASYNC_LOGGER_HPP
