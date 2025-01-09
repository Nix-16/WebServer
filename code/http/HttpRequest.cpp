#include "HttpRequest.h"
#include <algorithm>
#include <cctype>
#include <iostream>

/**
 * @brief 一些静态资源名的示例
 */
const std::unordered_set<std::string> HttpRequest::DEFAULT_HTML{
    "/index", "/register", "/login", "/welcome", "/video", "/picture"};

/**
 * @brief 对应的标识
 */
const std::unordered_map<std::string, int> HttpRequest::DEFAULT_HTML_TAG{
    {"/register.html", 0},
    {"/login.html", 1},
};

/* =====================================================================
 * 构造 / 析构 / 初始化
 * ===================================================================== */

HttpRequest::HttpRequest()
{
    Init();
}

void HttpRequest::Init()
{
    state_ = REQUEST_LINE;
    method_.clear();
    path_.clear();
    version_.clear();
    body_.clear();
    header_.clear();
    post_.clear();
}

/* =====================================================================
 * 对外接口
 * ===================================================================== */

bool HttpRequest::parse(Buffer &buff)
{
    // 循环，从缓冲区里按行读取数据，然后依次解析 请求行 -> 头 -> 体
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() <= 0)
    { // 如果缓冲区没有数据，返回false
        return false;
    }

    while (buff.ReadableBytes() && state_ != FINISH)
    {
        if (state_ == BODY)
        {
            // 直接处理整个请求体
            std::string body(buff.Peek(), buff.Peek() + buff.ReadableBytes());
            ParseBody_(body);                    // 解析请求体
            buff.Retrieve(buff.ReadableBytes()); // 清空已读取的 body 数据
            state_ = FINISH;                     // BODY 解析完成，状态置为 FINISH
            break;
        }

        // 其他状态（REQUEST_LINE 和 HEADERS）
        const char *lineEnd = std::search(buff.Peek(), buff.Peek() + buff.ReadableBytes(), CRLF, CRLF + 2);
        if (lineEnd == buff.Peek() + buff.ReadableBytes())
        {
            // 没有找到 "\r\n"，等待更多数据
            break;
        }

        std::string line(buff.Peek(), lineEnd); // 提取当前行

        switch (state_)
        {
        case REQUEST_LINE:
            if (!ParseRequestLine_(line))
            {
                return false; // 解析请求行失败，返回false
            }
            ParsePath_(); // 解析路径（如果需要）
            // state_ = PARSE_STATE::HEADERS;
            break;

        case HEADERS:
            ParseHeader_(line); // 解析请求头

            // 检查是否遇到空行，表示请求头结束
            if (line.empty())
            {
                if (method_ == "POST")
                {
                    state_ = BODY; // 如果是 POST 请求，转到 BODY 状态
                }
                else
                {
                    state_ = FINISH; // 否则，解析完成
                }
            }
            break;

        default:
            break;
        }

        // 安全检查 lineEnd 是否有效
        if (lineEnd >= buff.Peek() && lineEnd <= buff.Peek() + buff.ReadableBytes())
        {
            size_t lineLength = lineEnd - buff.Peek() + 2; // 行长度，包括 \r\n
            buff.Retrieve(lineLength);                     // 从缓冲区中删除已读取的数据
        }
        else
        {
            // std::cerr << "Invalid lineEnd pointer detected." << std::endl;
            return false; // 如果 lineEnd 无效，返回 false 避免段错误
        }
    }

    return true;
}

std::string HttpRequest::path() const
{
    return path_;
}

void HttpRequest::path(const std::string &path)
{
    path_ = path;
}

std::string HttpRequest::method() const
{
    return method_;
}

std::string HttpRequest::version() const
{
    return version_;
}

std::string HttpRequest::GetPost(const std::string &key) const
{
    if (post_.count(key))
    {
        return post_.at(key);
    }
    return "";
}

bool HttpRequest::IsKeepAlive() const
{
    auto it = header_.find("Connection");
    if (it != header_.end())
    {
        return it->second == "keep-alive";
    }
    return false; // 默认为短连接
}

/* =====================================================================
 * 解析核心逻辑
 * ===================================================================== */

/**
 * @brief 解析请求行: 例如 "GET /index.html HTTP/1.1"
 */
bool HttpRequest::ParseRequestLine_(const std::string &line)
{
    // 正则：(\S+)匹配方法，(\S+)匹配路径，(HTTP/\d\.\d)匹配版本
    std::regex pattern(R"((GET|POST) (\S+) (HTTP/\d\.\d))");
    std::smatch match;
    if (std::regex_match(line, match, pattern))
    {
        method_ = match[1];
        path_ = match[2];
        version_ = match[3];
        return true;
    }
    return false;
}

/**
 * @brief 解析请求头：形如 "Connection: keep-alive"
 */
void HttpRequest::ParseHeader_(const std::string &line)
{
    size_t pos = line.find(':');
    if (pos != std::string::npos)
    {
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 2); // 忽略": "部分
        header_[key] = value;
    }
}

/**
 * @brief 解析请求体
 */
void HttpRequest::ParseBody_(const std::string &line)
{
    body_ = line;
    ParsePost_();

    // 状态置为 FINISH
    state_ = FINISH;
}

/* =====================================================================
 * POST / 表单 / URL 编码等
 * ===================================================================== */

void HttpRequest::ParsePost_()
{
    //  根据 header["content-type"] 判断表单类型：application/x-www-form-urlencoded 或 multipart/form-data
    if (method_ == "POST" && header_["Content-Type"] == "application/x-www-form-urlencoded")
    {

        ParseFromUrlencoded_();

        if (DEFAULT_HTML_TAG.count(path_))
        {
            int tag = DEFAULT_HTML_TAG.find(path_)->second;
            if (tag == 0 || tag == 1)
            {
                bool isLogin = (tag == 1);
                if (UserVerify(post_["username"], post_["password"], isLogin))
                {
                    path_ = "/welcome.html";
                }
                else
                {
                    path_ = "/error.html";
                }
            }
        }
    }
}

void HttpRequest::ParseFromUrlencoded_()
{
    if (body_.size() == 0)
    {
        return;
    }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; i++)
    {
        char ch = body_[i];
        switch (ch)
        {
        case '=':
            key = body_.substr(j, i - j);
            j = i + 1;
            break;
        case '+':
            body_[i] = ' ';
            break;
        case '%':
            num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 2]);
            body_[i + 2] = num % 10 + '0';
            body_[i + 1] = num / 10 + '0';
            i += 2;
            break;
        case '&':
            value = body_.substr(j, i - j);
            j = i + 1;
            post_[key] = value;
            break;
        default:
            break;
        }
    }
    if (post_.count(key) == 0 && j < i)
    {
        value = body_.substr(j, i - j);
        post_[key] = value;
    }
}

/* =====================================================================
 * 解析路径 / 资源等
 * ===================================================================== */

void HttpRequest::ParsePath_()
{
    if (path_ == "/")
    {
        path_ = "/index.html";
    }
    else
    {
        for (auto &item : DEFAULT_HTML)
        {
            if (item == path_)
            {
                path_ += ".html";
                break;
            }
        }
    }
    this->state_ = PARSE_STATE::HEADERS;
}

/* =====================================================================
 * 工具 / 辅助函数
 * ===================================================================== */

int HttpRequest::ConverHex(char ch)
{
    // '0'~'9' => 0~9; 'A'~'F' => 10~15; 'a'~'f' => 10~15
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    else if (ch >= 'A' && ch <= 'F')
    {
        return ch - 'A' + 10;
    }
    else if (ch >= 'a' && ch <= 'f')
    {
        return ch - 'a' + 10;
    }
    return -1;
}

/* =====================================================================
 * 用户验证逻辑
 * ===================================================================== */

bool HttpRequest::UserVerify(const std::string &name, const std::string &pwd, bool isLogin)
{
    if (name.empty() || pwd.empty())
    {
        return false;
    }
    // RAII 方式获取一个数据库连接
    MYSQL *sql = nullptr;
    SqlConnRAII mysqlConn(&sql, SqlConnPool::Instance());
    if (!sql)
    {
        return false;
    }

    bool flag = false;
    char order[256] = {0};
    MYSQL_RES *res = nullptr;

    // 登录
    if (isLogin)
    {
        snprintf(order, 256, "SELECT password FROM user WHERE username='%s' LIMIT 1", name.c_str());

        if (mysql_query(sql, order))
        {
            std::cerr << "Login query failed: " << mysql_error(sql) << std::endl;
            SqlConnPool::Instance()->FreeConn(sql);
            return false;
        }

        res = mysql_store_result(sql);
        if (!res)
        {
            std::cerr << "Failed to store result: " << mysql_error(sql) << std::endl;
            SqlConnPool::Instance()->FreeConn(sql);
            return false;
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        if (row)
        {
            std::string password(row[0]);
            if (pwd == password)
            {
                flag = true;
            }
        }
        mysql_free_result(res);
    }
    // 注册逻辑
    else
    {
        snprintf(order, 256, "SELECT username FROM user WHERE username='%s' LIMIT 1", name.c_str());

        if (mysql_query(sql, order))
        {
            std::cerr << "Register check query failed: " << mysql_error(sql) << std::endl;
            SqlConnPool::Instance()->FreeConn(sql);
            return false;
        }

        res = mysql_store_result(sql);
        if (!res)
        {
            std::cerr << "Failed to store result: " << mysql_error(sql) << std::endl;
            SqlConnPool::Instance()->FreeConn(sql);
            return false;
        }

        if (!mysql_fetch_row(res))
        { // 用户不存在，可以注册
            snprintf(order, 256, "INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
            if (mysql_query(sql, order))
            {
                std::cerr << "User registration failed: " << mysql_error(sql) << std::endl;
                flag = false;
            }
            else
            {
                flag = true; // 注册成功
            }
        }
        else
        {
            std::cerr << "Username already exists." << std::endl;
            flag = false;
        }
        mysql_free_result(res);
    }
    
    SqlConnPool::Instance()->FreeConn(sql);
    return flag;
}
