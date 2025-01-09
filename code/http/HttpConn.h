#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <arpa/inet.h> // sockaddr_in
#include <sys/types.h>
#include <sys/uio.h> // readv, writev
#include <atomic>    // std::atomic
#include "../buffer/Buffer.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "config.h"

/**
 * @brief HttpConn 连接类，管理一个客户端 socket 的 HTTP 请求/响应
 */
class HttpConn
{
public:
    HttpConn();
    ~HttpConn();

    /**
     * @brief 初始化连接
     * @param sockFd 客户端 socket fd
     * @param addr   客户端地址
     */
    void init(int sockFd, const sockaddr_in &addr);

    /**
     * @brief 关闭连接
     */
    void Close();

    /**
     * @brief 从 fd 中读取数据
     * @param saveErrno 若发生错误，将错误码写入该指针
     * @return 读取字节数；若 0 表示对端关闭；-1 表示发生错误
     */
    ssize_t read(int* saveErrno);

    /**
     * @brief 向 fd 写出数据(支持多块缓冲：头部+文件映射)
     * @param saveErrno 若发生错误，将错误码写入该指针
     * @return 写出字节数；若 -1 且 errno==EAGAIN/WBLOCK，需要等待下次可写事件
     */
    ssize_t write(int *saveErrno);

    /**
     * @brief 返回连接对应的 socket fd
     */
    int GetFd() const;

    /**
     * @brief 返回客户端端口
     */
    int GetPort() const;

    /**
     * @brief 返回客户端 IP (字符串形式)
     */
    const char *GetIP() const;

    /**
     * @brief 返回客户端地址结构
     */
    sockaddr_in GetAddr() const;

    /**
     * @brief 解析请求并生成响应
     * @return 若需要写响应数据返回 true，否则 false
     */
    bool process();

    /**
     * @brief 剩余待写字节数（含响应头和文件映射部分）
     */
    int ToWriteBytes() const
    {
        return iov_[0].iov_len + iov_[1].iov_len;
    }

    /**
     * @brief 是否保持长连接
     */
    bool IsKeepAlive() const
    {
        return request_.IsKeepAlive();
    }

    /**
     * @brief 是否在写数据（如使用 EPOLLONESHOT 需要判断）
     */
    bool IsWriting() const { return isWriting_; }

    /**
     * @brief 设置是否在写数据
     */
    void SetWriting(bool flag) { isWriting_ = flag; }

public:
    /**
     * @brief 资源目录
     * 一般在项目启动时由框架设定，或直接在 CMake/工程里写死
     */
    static const char *srcDir;

    /**
     * @brief 全局的活跃连接数
     */
    static std::atomic<int> userCount;

private:
    bool isWriting_; // 是否正在写数据
    bool isClose_;   // 连接是否已关闭

    int fd_;           // 客户端 socket
    sockaddr_in addr_; // 客户端地址

    // iovec 相关：一次 writev() 可以写两块数据
    //  - iov_[0]: 存放响应头和部分内容
    //  - iov_[1]: 可能是文件映射的内容(如静态文件)
    struct iovec iov_[2];
    int iovCnt_;

    Buffer readBuff_;  // 读缓冲
    Buffer writeBuff_; // 写缓冲

    HttpRequest request_;   // HTTP 请求
    HttpResponse response_; // HTTP 响应
};

#endif // HTTP_CONN_H
