#include "HttpConn.h"
#include <unistd.h>     // close()
#include <sys/socket.h> // recv(), send()
#include <fcntl.h>
#include <errno.h>
#include <cassert>
#include <atomic>
#include <iostream> // 可选：日志/调试输出

// 静态成员定义
const char *HttpConn::srcDir = nullptr;
std::atomic<int> HttpConn::userCount{0};

HttpConn::HttpConn()
    : isWriting_(false),
      isClose_(true),
      fd_(-1),
      iovCnt_(0)
{
    // 初始化内存
    iov_[0].iov_base = nullptr;
    iov_[0].iov_len = 0;
    iov_[1].iov_base = nullptr;
    iov_[1].iov_len = 0;

    // 获取配置的单例实例
    Config &config = Config::GetInstance();
    srcDir = config.GetServerSrcDir().c_str();
}

HttpConn::~HttpConn()
{
    Close();
}

void HttpConn::init(int sockFd, const sockaddr_in &addr)
{
    assert(sockFd > 0);
    fd_ = sockFd;
    addr_ = addr;
    // 每次新连接进来，活跃用户数+1
    userCount++;

    isClose_ = false;
    isWriting_ = false;
    iovCnt_ = 0;

    // 缓冲区/请求/响应初始化
    readBuff_.Clear();
    writeBuff_.Clear();
    request_.Init();
    response_.UnmapFile();

    // 设置默认资源目录(如果你的项目不需要动态修改，可直接在 HttpConn.h 中写死)
    // HttpConn::srcDir = "/home/xxx/your_project/resources"; // 也可在 main 中初始化
}

void HttpConn::Close()
{
    if (!isClose_)
    {
        isClose_ = true;
        userCount--;
        if (fd_ >= 0)
            close(fd_);
        fd_ = -1;
    }
}

int HttpConn::GetFd() const
{
    return fd_;
}

int HttpConn::GetPort() const
{
    return ntohs(addr_.sin_port);
}

const char *HttpConn::GetIP() const
{
    return inet_ntoa(addr_.sin_addr);
}

sockaddr_in HttpConn::GetAddr() const
{
    return addr_;
}

/**
 * @brief 从 socket 中读取数据，存入 readBuff_
 * @param saveErrno 若发生错误，记录在此
 * @return 读取字节数；
 */
ssize_t HttpConn::read(int *saveErrno)
{
    if (isClose_)
    {
        return 0;
    }

    ssize_t len = -1;
    ssize_t totalLen = 0;
    while (true)
    {
        len = readBuff_.ReadFd(fd_, saveErrno);
        totalLen += len;
        if (len <= 0)
        {
            break;
        }
    }

    return totalLen;
}

/**
 * @brief 将响应数据写到 socket，支持 writev 合并发送头部和映射文件
 * @param saveErrno 若发生错误，记录在此
 * @return 累计写了多少
 */
ssize_t HttpConn::write(int *saveErrno)
{
    if (isClose_)
    {
        return 0;
    }
    ssize_t totalLen = 0; // 记录总共写入的字节数
    ssize_t len = -1;

    while (true)
    {
        len = writev(fd_, iov_, iovCnt_); // 执行写操作
        totalLen += len;                  // 累计写入的字节数

        if (len <= 0)
        {
            *saveErrno = errno;
            break;
        }
        if (iov_[0].iov_len + iov_[1].iov_len == 0)
        {
            break;
        } /* 传输结束 */
        else if (static_cast<size_t>(len) > iov_[0].iov_len)
        {
            iov_[1].iov_base = (uint8_t *)iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if (iov_[0].iov_len)
            {
                writeBuff_.Clear();
                iov_[0].iov_len = 0;
            }
        }
        else
        {
            iov_[0].iov_base = (uint8_t *)iov_[0].iov_base + len;
            iov_[0].iov_len -= len;
            writeBuff_.Retrieve(len);
        }
    }

    return totalLen; // 返回总共写入的字节数
}

/**
 * @brief 解析请求，并生成响应
 * @return 若解析成功且有数据需要发送，返回 true，否则 false
 */
bool HttpConn::process()
{
    request_.Init();
    // 1. 解析请求(把 readBuff_ 中的数据解析到 request_)
    if (readBuff_.ReadableBytes() <= 0)
    {
        return false;
    }
    bool ret = request_.parse(readBuff_);

    if (!ret)
    {
        // 解析失败 => 返回 400
        response_.Init(srcDir, request_.path(), false, 400);
        return false;
    }
    else
    {
        // 解析成功 => 构造 response
        response_.Init(srcDir, request_.path(), request_.IsKeepAlive(), 200);
    }

    // 2. 生成响应头(写入 writeBuff_), 并 mmap 文件(若需要)
    response_.MakeResponse(writeBuff_);

    // 3. 准备 iovec: [0] => writeBuff_  [1] => mmap file
    //    在 write() 时统一发送
    iov_[0].iov_base = const_cast<char *>(writeBuff_.Peek());
    iov_[0].iov_len = writeBuff_.ReadableBytes();
    iovCnt_ = 1;

    if (response_.FileLen() > 0 && response_.File())
    {
        iov_[1].iov_base = response_.File();
        iov_[1].iov_len = response_.FileLen();
        iovCnt_ = 2;
    }

    return true; // 有响应要发送
}
