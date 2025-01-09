#ifndef BUFFER_H
#define BUFFER_H

#include <vector>
#include <string>
#include <cstring>
#include <cassert>

/**
 * @brief 以连续内存实现的动态 Buffer
 *  - readPos_：当前可读数据的起始位置
 *  - writePos_：当前可写区域的起始位置
 *  - 当剩余可写空间不足时，会自动扩容
 */
class Buffer
{
public:
    /**
     * @brief 默认构造函数，初始容量可以自行设置
     */
    explicit Buffer(size_t initBuffSize = 1024);

    ~Buffer() = default;

    /**
     * @return 可读数据大小
     */
    size_t ReadableBytes() const;

    /**
     * @return 可写空间大小
     */
    size_t WritableBytes() const;

    /**
     * @return 已经弃用的空间大小(从 buffer 开始到 readPos_)
     * 这个空间可能被复用给写区域
     */
    size_t PrependableBytes() const;

    /**
     * @brief 返回当前可读数据的首地址指针
     */
    const char *Peek() const;

    /**
     * @brief 返回可写区域的首地址指针
     */
    char *BeginWrite();

    /**
     * @brief const版本，可写区域的首地址指针
     */
    const char *BeginWriteConst() const;

    /**
     * @brief 更新写指针位置(向后移动 len)
     */
    void HasWritten(size_t len);

    /**
     * @brief 取出 [readPos_, readPos_+len) 这段数据，然后移动 readPos_
     * @param len 要取出的长度
     */
    void Retrieve(size_t len);

    /**
     * @brief 取出直到 end 指针位置的数据
     */
    void RetrieveUntil(const char *end);

    /**
     * @brief 清空所有数据: readPos_ = writePos_ = 0
     */
    void Clear();

    /**
     * @brief 返回已读完的数据(以 std::string 返回)，并移动 readPos_ 到写指针
     */
    std::string RetrieveAllToStr();

    /**
     * @return std::string：把当前可读数据拷贝成一个字符串
     */
    std::string ToString() const;

    /**
     * @brief 将 [str, str+len) 数据添加到写缓冲
     */
    void Append(const char *str, size_t len);

    /**
     * @brief 重载：Append(const std::string&)
     */
    void Append(const std::string &str);

    /**
     * @brief 追加另一块Buffer的可读数据
     */
    void Append(const Buffer &buff);

    /**
     * @brief 从文件描述符（例如套接字）读取数据并将其存储到缓冲区
     */
    ssize_t ReadFd(int fd, int *Errno);
    ssize_t WriteFd(int fd, int *Errno);

private:
    /**
     * @brief 缓冲区扩容的核心函数
     *        - 若前面已经读走很多数据，可以先腾挪(把 unread 数据前移)
     *        - 若还是不够，再扩容
     */
    void MakeSpace(size_t len);

    /**
     * @brief 返回底层 vector 的首地址
     */
    char *BeginPtr_();

    /**
     * @brief 返回底层 vector 的首地址(const)
     */
    const char *BeginPtr_() const;

private:
    std::vector<char> buffer_; // 底层实际缓冲
    size_t readPos_;           // 当前可读位置
    size_t writePos_;          // 当前可写位置
};

#endif // BUFFER_H
