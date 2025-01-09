#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <unordered_map>
#include <fcntl.h>    // open
#include <unistd.h>   // close
#include <sys/stat.h> // stat
#include <sys/mman.h> // mmap, munmap
#include <string>

/**
 * 前置声明：你的 Buffer 类。请根据自己的项目路径做相应修改。
 */
#include "../buffer/Buffer.h"

/**
 * @brief HttpResponse：用于组装 HTTP 响应（状态行、头部、正文）。
 *        在正文部分，支持通过 mmap 映射静态文件。
 */
class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    /**
     * @brief 初始化响应对象
     * @param srcDir       资源根目录(如"/home/user/www")
     * @param path         请求的资源相对路径(如"/index.html")
     * @param isKeepAlive  是否长连接
     * @param code         HTTP 状态码(若为 -1，会在后续自动根据文件情况设置)
     */
    void Init(const std::string &srcDir,
              const std::string &path,
              bool isKeepAlive = false,
              int code = -1);

    /**
     * @brief 根据当前设定的状态码、文件路径等信息，往 buff 写出完整的响应(行、头、正文)。
     * @param buff 传入的缓冲区，用于存放要发送的响应头部数据
     */
    void MakeResponse(Buffer &buff);

    /**
     * @brief 释放 mmap 映射(若有)
     */
    void UnmapFile();

    /**
     * @brief 返回已映射文件的首地址(用于 writev)
     */
    char *File();

    /**
     * @brief 返回已映射文件的大小(用于 writev)
     */
    size_t FileLen() const;

    /**
     * @brief 写入一段简易的 HTML 来描述错误信息
     * @param buff    响应头要写入的缓冲
     * @param message 错误描述
     */
    void ErrorContent(Buffer &buff, const std::string &message);

    /**
     * @brief 返回当前的 HTTP 状态码
     */
    int Code() const { return code_; }

private:
    /**
     * @brief 添加响应行，如"HTTP/1.1 200 OK\r\n"
     */
    void AddStateLine_(Buffer &buff);

    /**
     * @brief 添加响应头，如"Connection: keep-alive\r\nContent-Type: ...\r\n..."
     */
    void AddHeader_(Buffer &buff);

    /**
     * @brief 添加正文部分(若无错误码，就 mmap 文件；否则写错误页面)
     */
    void AddContent_(Buffer &buff);

    /**
     * @brief 根据 path_ 的后缀来判断返回的Content-Type
     */
    std::string GetFileType_();

private:
    int code_;         // HTTP状态码，如 200,404 等
    bool isKeepAlive_; // 是否长连接

    std::string path_;   // 请求的资源路径(如"/index.html")
    std::string srcDir_; // 资源根目录

    char *mmFile_;           // mmap 映射文件的首地址
    struct stat mmFileStat_; // mmap 文件的 stat 信息(大小/权限等)

    // 后缀 -> MIME类型
    static const std::unordered_map<std::string, std::string> SUFFIX_TYPE;
    // 状态码 -> 状态描述
    static const std::unordered_map<int, std::string> CODE_STATUS;
    // 部分错误码 -> 错误页面对应路径
    static const std::unordered_map<int, std::string> CODE_PATH;
};

#endif // HTTP_RESPONSE_H
