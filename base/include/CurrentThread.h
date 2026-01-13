#pragma once

namespace CurrentThread
{   
    // extern声明外部链接的变量，跨文件共享全局变量
    // __thread是将这个变量进行设置每个线程中都有一份拷贝，可以进行更改不影响其他
    extern __thread int t_cachedTid;

    void cacheTid();

    inline int tid()
    {   
        // 如果等于零则使用cacheTid通过系统调用获取当前线程Tid
        if (__builtin_expect(t_cachedTid == 0, 0))
        {
            cacheTid();
        }
        // 有则直接返回
        return t_cachedTid;
    }
}