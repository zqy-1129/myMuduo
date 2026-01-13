#include "Buffer.h"

#include <sys/uio.h>
#include <unistd.h>

/**
 *  从fd上读取数据，Poller工作在LT模式上  
 *  Buffer缓冲区是有大小的，但是从fd上读取的时候，不确定数据大小
 */ 
ssize_t Buffer::readFd(int fd, int* saveErrno)
{       
    char extrabuf[65535] = {0};  // 开辟在栈上的空间 64K，这个空间是自动释放的
    struct iovec vec[2];

    // Buffer底层缓冲区剩余可写大小
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof extrabuf;

    const int iovcnt = (writable < sizeof extrabuf) ? 2 : 1;
    // 从fd上读取的数据
    const ssize_t n = ::readv(fd, vec, iovcnt);

    if (n < 0)
    {
        *saveErrno = errno;
    }
    else if (n <= writable)     // 原始buffer够写
    {
        writerIndex_ += n;
    }
    else{
        // 数据大于原本的缓冲区写区大小
        writerIndex_ = buffer_.size();
        // 将额外空间的数据写入buffer中，结束后额外空间自动释放
        append(extrabuf, n - writable);
    }
    return n;
}


// 向fd中写
ssize_t Buffer::writeFd(int fd, int* saveErrno)
{
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n <= 0)
    {
        *saveErrno = errno;
    }
    return n;
}

Buffer::~Buffer()
{
    // 默认vector会自动释放内存
}