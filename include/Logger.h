#pragma once
#include <plog/Log.h>
#include <plog/Initializers/RollingFileInitializer.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ColorConsoleAppender.h>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

inline std::string generateFilename() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);   // Windows 安全版本
#else
    localtime_r(&t, &tm);   // Linux/Unix 线程安全版本
#endif
    char buf[64]; // 足够大的缓冲区
    if (std::strftime(buf, sizeof(buf), "%Y%m%d-%H.log", &tm) == 0) {
        // 0 表示转换失败
        return "log_unknown.log";
    }
    return std::string(buf);
}

//plog::info, plog::debug
inline void LogInit(int level, int rotationSize, int leftFileNum)
{
    std::string filename = generateFilename();
    // 步骤1: 初始化文件日志 - 创建滚动日志文件
    // 参数: 日志级别, 文件名, 滚动文件大小(字节), 保留文件数量
    plog::init(plog::Severity(level), filename.c_str(), rotationSize, leftFileNum);

    // 步骤2: 添加彩色控制台输出
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::get()->addAppender(&consoleAppender);
}
