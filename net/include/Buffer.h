#pragma once

#include "noncopyable.h"

#include <vector>
#include <string>
#include <algorithm>

/// A buffer class modeled after org.jboss.netty.buffer.ChannelBuffer
///
/// @code
/// +-------------------+------------------+------------------+
/// | prependable bytes |  readable bytes  |  writable bytes  |
/// |                   |     (CONTENT)    |                  |
/// +-------------------+------------------+------------------+
/// |                   |                  |                  |
/// 0      <=      readerIndex   <=   writerIndex    <=     size
/// @endcode

// 这里的缓冲区是有人往里写，写完readable数据就会增加，读是读取写进去的数据

class Buffer : noncopyable
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

    explicit Buffer(size_t initialSize = kInitialSize)
        : buffer_(kCheapPrepend + initialSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
    {}

    ~Buffer();

    size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    size_t writableBytes() const 
    {
        return buffer_.size() - writerIndex_;
    }

    size_t prependableBytes() const
    {
        return readerIndex_;
    }

    // 返回可读数据缓冲区的起始地址
    const char* peek() const 
    {
        return begin() + readerIndex_;
    }

    // onMassage string <- Buffer
    void retrieve(size_t len)
    {
        if (len < readableBytes())
        {
            readerIndex_ += len;    // 说明有没有读完的
        }
        else 
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    std::string retrieveAllAsString()
    {
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(size_t len)
    {
        std::string result(peek(), len);
        // 缓冲区的复位操作
        retrieve(len);
        return result;
    }

    void ensureWritableBytes(size_t len)
    {
        if (writableBytes() < len)
        {
            makeSpace(len);
        }
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    void append(const char* data, size_t len) 
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    ssize_t readFd(int fd, int* saveErrno);

    ssize_t writeFd(int fd, int* saveErrno);

private:
    char* begin()
    {   
        // 这里是先调用迭代器的*的重载函数得到第一个值，之后在取地址得到首地址
        return &*buffer_.begin(); 
    }

    const char* begin() const
    {   
        // 这里是先调用迭代器的*的重载函数得到第一个值，之后在取地址得到首地址
        return &*buffer_.begin(); 
    }

    // 扩容操作
    void makeSpace(size_t len)
    {
        if (writableBytes() + prependableBytes() - kCheapPrepend < len)
        {
            buffer_.resize(len + writerIndex_);
        }
        else 
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_, 
                    begin() + writerIndex_,
                    begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    size_t readerIndex_;
    size_t writerIndex_;
};