#include "Buffer.h"
#include <cassert>
#include <cstring>
#include <sys/uio.h>
#include <unistd.h>

// 构造函数
Buffer::Buffer(size_t initBuffSize)
    : buffer_(initBuffSize),
      readPos_(0),
      writePos_(0)
{
}

/* ------------------- 基本属性 -------------------- */
size_t Buffer::ReadableBytes() const
{
    return writePos_ - readPos_;
}

size_t Buffer::WritableBytes() const
{
    return buffer_.size() - writePos_;
}

size_t Buffer::PrependableBytes() const
{
    return readPos_;
}

/* ------------------- 读指针 / 写指针 -------------------- */
const char *Buffer::Peek() const
{
    // 可读数据的起始地址
    return BeginPtr_() + readPos_;
}

char *Buffer::BeginWrite()
{
    // 可写区域的起始地址
    return BeginPtr_() + writePos_;
}

const char *Buffer::BeginWriteConst() const
{
    return BeginPtr_() + writePos_;
}

void Buffer::HasWritten(size_t len)
{
    writePos_ += len;
}

/* ------------------- Retrieve / 清理 -------------------- */
void Buffer::Retrieve(size_t len)
{
    assert(len <= ReadableBytes());
    if (len < ReadableBytes())
    {
        // 仅移动 readPos_
        readPos_ += len;
    }
    else
    {
        // 读完所有可读数据，就相当于清空
        Clear();
    }
}

void Buffer::RetrieveUntil(const char *end)
{
    assert(Peek() <= end);
    assert(end <= BeginWriteConst());
    Retrieve(end - Peek());
}

void Buffer::Clear()
{
    // 不再调用 bzero，以免多余性能开销
    // 仅重置指针
    readPos_ = 0;
    writePos_ = 0;
}

/* ------------------- 字符串 / 数据操作 -------------------- */
std::string Buffer::RetrieveAllToStr()
{
    std::string str(Peek(), ReadableBytes());
    Clear();
    return str;
}

std::string Buffer::ToString() const
{
    return std::string(Peek(), ReadableBytes());
}

void Buffer::Append(const char *str, size_t len)
{
    if (!str || len == 0)
    {
        return;
    }
    MakeSpace(len);
    std::memcpy(BeginWrite(), str, len);
    HasWritten(len);
}

void Buffer::Append(const std::string &str)
{
    Append(str.data(), str.size());
}

void Buffer::Append(const Buffer &buff)
{
    Append(buff.Peek(), buff.ReadableBytes());
}

/* ------------------- 读写系统接口 -------------------- */

// 从文件描述符读取数据并存储到缓冲区
ssize_t Buffer::ReadFd(int fd, int *Errno)
{
    // 临时缓冲区，用于防止空间不足
    char extra_buffer[65536];
    struct iovec iov[2];

    size_t writable = WritableBytes();
    // 第一块缓冲区：指向当前缓冲区的可写区域
    iov[0].iov_base = BeginWrite();
    iov[0].iov_len = writable;

    // 第二块缓冲区：临时缓冲区
    iov[1].iov_base = extra_buffer;
    iov[1].iov_len = sizeof(extra_buffer);

    // 一次性尝试读到两块里
    ssize_t n = readv(fd, iov, 2);
    if (n < 0)
    {
        *Errno = errno;
    }
    else if (static_cast<size_t>(n) <= writable)
    {
        // 全部数据读进了 buffer_ 的可写区
        writePos_ += n;
    }
    else
    {
        // 第一块写满，需要把多余数据放到临时缓冲再 Append
        writePos_ = buffer_.size();
        Append(extra_buffer, n - writable);
    }
    return n;
}

// 将缓冲区数据写入文件描述符
ssize_t Buffer::WriteFd(int fd, int *Errno)
{
    size_t readable = ReadableBytes();
    ssize_t n = write(fd, Peek(), readable);
    if (n < 0)
    {
        *Errno = errno;
        return n;
    }
    // 向前移动 readPos_
    readPos_ += n;
    // 如果把可读数据全部写完，则清空
    if (readPos_ == writePos_)
    {
        Clear();
    }
    return n;
}

/* ------------------- 内部辅助 -------------------- */

void Buffer::MakeSpace(size_t len)
{
    // 如果可写空间 >= 需要的 len，就不用扩容
    if (WritableBytes() >= len)
    {
        return;
    }
    // 否则看能否通过“前移”腾出空间
    size_t needed = len - WritableBytes();
    if (PrependableBytes() + WritableBytes() >= len)
    {
        // 把可读数据前移到 buffer 起始
        size_t readable = ReadableBytes();
        std::memmove(BeginPtr_(), Peek(), readable);
        readPos_ = 0;
        writePos_ = readable;
    }
    else
    {
        // 不够，直接扩容
        buffer_.resize(buffer_.size() + needed);
    }
}

char *Buffer::BeginPtr_()
{
    return &*buffer_.begin();
}

const char *Buffer::BeginPtr_() const
{
    return &*buffer_.begin();
}
