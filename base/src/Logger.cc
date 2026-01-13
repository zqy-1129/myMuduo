#include "Logger.h"
#include "Timestamp.h"



// 获取日志的唯一实例（单例模式）
Logger& Logger::instance() 
{
    static Logger logger;
    return logger;
}

// 设置日志级别
void Logger::setLogLevel(int Level) 
{
    logLevel_ = Level;
}

// 写日志 [级别信息] [time] : [msg]
void Logger::log(std::string msg) 
{   
    // 打印消息级别
    switch (logLevel_)
    {
        case INFO:
            std::cout << "[INFO]";
            break;
        case ERROR:
            std::cout << "[ERROR]";
            break;
        case FATAL:
            std::cout << "[FATAL]";
            break;
        case DEBUG:
            std::cout << "[DEBUG]";
            break;
        default:
            break;
    }

    // 打印时间和消息
    std::cout << Timestamp::now().toString() << " : " << msg << std::endl;
}
