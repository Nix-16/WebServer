#include "HttpResponse.h"
#include <cassert>
#include <cstring>
#include <iostream>

// 不同文件后缀 -> MIME 类型
const std::unordered_map<std::string, std::string> HttpResponse::SUFFIX_TYPE = {
    {".html", "text/html"},
    {".xml", "text/xml"},
    {".xhtml", "application/xhtml+xml"},
    {".txt", "text/plain"},
    {".rtf", "application/rtf"},
    {".pdf", "application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".css", "text/css"},
    {".js", "text/javascript"}};

// 状态码 -> 状态描述
const std::unordered_map<int, std::string> HttpResponse::CODE_STATUS = {
    {200, "OK"},
    {400, "Bad Request"},
    {403, "Forbidden"},
    {404, "Not Found"},
    {500, "Internal Server Error"}};

// 部分错误码 -> 错误页面路径(相对 srcDir_)
const std::unordered_map<int, std::string> HttpResponse::CODE_PATH = {
    {400, "/400.html"},
    {403, "/403.html"},
    {404, "/404.html"},
    {500, "/500.html"}};

HttpResponse::HttpResponse()
    : code_(-1),
      isKeepAlive_(false),
      path_(""),
      srcDir_(""),
      mmFile_(nullptr)
{
    memset(&mmFileStat_, 0, sizeof(mmFileStat_));
}

HttpResponse::~HttpResponse()
{
    UnmapFile();
}

void HttpResponse::Init(const std::string &srcDir, const std::string &path, bool isKeepAlive, int code)
{
    // 如果之前已映射过文件，先释放
    if (mmFile_)
    {
        UnmapFile();
    }
    srcDir_ = srcDir;
    path_ = path;
    isKeepAlive_ = isKeepAlive;
    code_ = code;

    // 重置文件映射信息
    mmFile_ = nullptr;
    memset(&mmFileStat_, 0, sizeof(mmFileStat_));
}

void HttpResponse::MakeResponse(Buffer &buff)
{
    // 1. 检测文件状态
    if (stat((srcDir_ + path_).data(), &mmFileStat_) < 0 || S_ISDIR(mmFileStat_.st_mode))
    {
        // 文件不存在 或者 path 指向目录
        code_ = 404;
    }
    else if (!(mmFileStat_.st_mode & S_IROTH))
    {
        // 文件不可读 => 403
        code_ = 403;
    }
    else if (code_ == -1)
    {
        // 如果用户未指定 code, 默认 200
        code_ = 200;
    }

    // 2. 如果是错误码(如404), 替换成对应的错误页面
    if (CODE_PATH.count(code_))
    {
        path_ = CODE_PATH.find(code_)->second;
        // 再次 stat
        stat((srcDir_ + path_).data(), &mmFileStat_);
    }

    // 3. 写响应行
    AddStateLine_(buff);
    // 4. 写响应头
    AddHeader_(buff);
    // 5. 写正文(可能是 mmap 文件，也可能是简易错误内容)
    AddContent_(buff);
}

void HttpResponse::UnmapFile()
{
    if (mmFile_)
    {
        munmap(mmFile_, mmFileStat_.st_size);
        mmFile_ = nullptr;
    }
}

char *HttpResponse::File()
{
    return mmFile_;
}

size_t HttpResponse::FileLen() const
{
    return mmFileStat_.st_size;
}

void HttpResponse::ErrorContent(Buffer &buff, const std::string &message)
{
    // 拼装一个简单的错误 HTML 页面
    std::string body;
    body = "<html><title>Error</title>";
    body += "<body bgcolor=\"ffffff\">";
    body += std::to_string(code_) + " : " + CODE_STATUS.find(code_)->second + "\n";
    body += "<p>" + message + "</p>";
    body += "<hr><em>My WebServer</em></body></html>";

    // 写响应行 + 头 + 错误页正文
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + CODE_STATUS.find(code_)->second + "\r\n");
    buff.Append("Content-type: text/html\r\n");
    buff.Append("Content-length: " + std::to_string(body.size()) + "\r\n");
    buff.Append("Connection: close\r\n\r\n");
    buff.Append(body);
}

void HttpResponse::AddStateLine_(Buffer &buff)
{
    std::string status;
    if (CODE_STATUS.count(code_))
    {
        status = CODE_STATUS.find(code_)->second;
    }
    else
    {
        // 若未找到对应的描述，则认为是 400
        code_ = 400;
        status = "Bad Request";
    }
    // 例如写入: HTTP/1.1 200 OK\r\n
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + status + "\r\n");
}

void HttpResponse::AddHeader_(Buffer &buff)
{
    buff.Append("Connection: ");
    if (isKeepAlive_)
    {
        buff.Append("keep-alive\r\n");
        // 可根据需要追加更多 keep-alive 信息
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    }
    else
    {
        buff.Append("close\r\n");
    }

    // Content-Type: ...
    buff.Append("Content-Type: " + GetFileType_() + "\r\n");
}

void HttpResponse::AddContent_(Buffer &buff)
{
    // 打开文件
    int srcFd = open((srcDir_ + path_).data(), O_RDONLY);
    if (srcFd < 0)
    {
        // 如果无法打开文件，写入一个简单的错误提示
        ErrorContent(buff, "File Not Found: " + path_);
        return;
    }

    // 先写 Content-Length 行
    buff.Append("Content-Length: " + std::to_string(mmFileStat_.st_size) + "\r\n\r\n");

    // 使用 mmap
    mmFile_ = static_cast<char *>(mmap(nullptr, mmFileStat_.st_size, PROT_READ, MAP_PRIVATE, srcFd, 0));
    close(srcFd);

    if (mmFile_ == MAP_FAILED)
    {
        mmFile_ = nullptr;
        // 如果 mmap 失败，也写入错误提示
        ErrorContent(buff, "File Mapping Failed: " + path_);
        return;
    }

    // 正文部分(文件内容)不直接拷贝到 buff，而是在后续 writev 时一并发送
    // 所以这里不做 buff.Append()，只写了头部 + \r\n\r\n
}

std::string HttpResponse::GetFileType_()
{
    // 根据文件后缀判断 MIME 类型
    size_t idx = path_.find_last_of('.');
    if (idx == std::string::npos)
    {
        return "text/plain";
    }
    std::string suffix = path_.substr(idx);
    if (SUFFIX_TYPE.count(suffix))
    {
        return SUFFIX_TYPE.find(suffix)->second;
    }
    return "text/plain";
}
