#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_map>
#include <unordered_set>
#include <string>
#include <regex>
#include <sstream>
#include <mysql/mysql.h> // MySQL 连接池支持

#include "../buffer/Buffer.h"
#include "../pool/SqlConnRAII.h"
#include "../pool/SqlConnPool.h"

/**
 * @brief 表示一个 HTTP 请求的解析过程和结果
 */
class HttpRequest
{
public:
    /**
     * @brief 解析过程中的状态
     */
    enum PARSE_STATE
    {
        REQUEST_LINE, // 正在解析请求行
        HEADERS,      // 正在解析请求头
        BODY,         // 正在解析请求体
        FINISH,       // 解析完成
    };

    /**
     * @brief 解析结果状态码
     */
    enum HTTP_CODE
    {
        NO_REQUEST = 0,    // 数据不完整，还需继续接收
        GET_REQUEST,       // 解析到完整的 GET 请求
        BAD_REQUEST,       // 错误的请求
        NO_RESOURCE,       // 请求的资源不存在
        FORBIDDEN_REQUEST, // 无权限访问
        FILE_REQUEST,      // 静态文件请求
        INTERNAL_ERROR,    // 服务器内部错误
        CLOSED_CONNECTION, // 客户端关闭连接
    };

public:
    HttpRequest();
    ~HttpRequest() = default;

    /**
     * @brief 初始化或重置所有成员，以便解析新的请求
     */
    void Init();

    /**
     * @brief 从 buff 中解析 HTTP 请求内容
     * @param buff 存放请求数据的缓冲区
     * @return 是否解析成功
     */
    bool parse(Buffer &buff);

    /**
     * @brief 获取解析后的请求路径
     */
    std::string path() const;

    /**
     * @brief 设置(重置)请求路径
     */
    void path(const std::string &path);

    /**
     * @brief 获取请求方法(GET/POST等)
     */
    std::string method() const;

    /**
     * @brief 获取 HTTP 版本(HTTP/1.1)
     */
    std::string version() const;

    /**
     * @brief 获取表单中 key 对应的值(仅适用于 POST)
     */
    std::string GetPost(const std::string &key) const;

    /**
     * @brief 是否为长连接
     */
    bool IsKeepAlive() const;

    /**
     * @brief 静态函数，用于模拟用户验证或登录
     * @param name 用户名
     * @param pwd  密码
     * @param isLogin 判断是登录还是注册
     * @return 验证是否成功
     */
    static bool UserVerify(const std::string &name, const std::string &pwd, bool isLogin);

private:
    /**
     * @brief 解析请求行: GET /index.html HTTP/1.1
     */
    bool ParseRequestLine_(const std::string &line);

    /**
     * @brief 解析请求头
     */
    void ParseHeader_(const std::string &line);

    /**
     * @brief 解析请求体
     */
    void ParseBody_(const std::string &line);

    /**
     * @brief 补充解析：当 method_ == POST 时，解析表单数据
     */
    void ParsePost_();

    /**
     * @brief 解析路径(可能需要默认添加index.html等)
     */
    void ParsePath_();

    /**
     * @brief 解析 URL 中的 %xx 转义字符
     */
    void ParseFromUrlencoded_();

    /**
     * @brief 将十六进制字符转换为十进制
     */
    static int ConverHex(char ch);

private:
    PARSE_STATE state_;   // 状态机当前所处阶段
    std::string method_;  // 请求方法
    std::string path_;    // 请求目标路径
    std::string version_; // HTTP版本
    std::string body_;    // 请求体(POST 数据)

    // 请求头字段集合
    std::unordered_map<std::string, std::string> header_;
    // POST表单解析后存放的键值对
    std::unordered_map<std::string, std::string> post_;

    // 一些默认资源名配置(示例)
    static const std::unordered_set<std::string> DEFAULT_HTML;
    // 和一些资源对应的标识(示例)
    static const std::unordered_map<std::string, int> DEFAULT_HTML_TAG;
};

#endif // HTTP_REQUEST_H
