// Logging utilities and breakpoint request helpers
#pragma once

#include <cstdarg>
#include <cstdint>
#include <utility>

namespace KibakoEngine {

    enum class LogLevel : std::uint8_t
    {
        Trace,
        Info,
        Warning,
        Error,
        Critical
    };

    struct LogConfig
    {
        LogLevel minimumLevel = LogLevel::Trace;
        LogLevel debuggerBreakLevel = LogLevel::Critical;
#if defined(NDEBUG)
        bool breakIntoDebugger = false;
#else
        bool breakIntoDebugger = true;
#endif
        bool haltRenderingOnBreak = true;
    };

    void SetLogConfig(const LogConfig& config);
    LogConfig GetLogConfig();

    void RequestBreakpoint(const char* reason, LogLevel level = LogLevel::Error);

    bool HasBreakpointRequest();
    bool ConsumeBreakpointRequest();
    const char* LastBreakpointMessage();

    void LogMessage(LogLevel level,
                    const char* channel,
                    const char* file,
                    int line,
                    const char* function,
                    const char* fmt,
                    ...);
    void LogMessageV(LogLevel level,
                     const char* channel,
                     const char* file,
                     int line,
                     const char* function,
                     const char* fmt,
                     std::va_list args);

    namespace Detail
    {
        struct LogMessageContext
        {
            LogLevel level;
            const char* channel;
            const char* file;
            int line;
            const char* function;

            template <typename... Args>
            void operator()(const char* fmt, Args&&... args) const
            {
                LogMessage(level,
                           channel,
                           file,
                           line,
                           function,
                           fmt,
                           std::forward<Args>(args)...);
            }
        };

        inline LogMessageContext MakeLogMessageContext(LogLevel level,
                                                        const char* channel,
                                                        const char* file,
                                                        int line,
                                                        const char* function)
        {
            return LogMessageContext{level, channel, file, line, function};
        }
    } // namespace Detail

} // namespace KibakoEngine

#define KBK_LOG_CHANNEL_DEFAULT "Kibako"

#define KBK_LOG(level, channel, ...)                                                                           \
    ::KibakoEngine::Detail::MakeLogMessageContext((level),                                                    \
                                                  (channel),                                                  \
                                                  __FILE__,                                                   \
                                                  __LINE__,                                                   \
                                                  __func__)(__VA_ARGS__)

#define KbkTrace(channel, ...)   KBK_LOG(::KibakoEngine::LogLevel::Trace, (channel), __VA_ARGS__)
#define KbkLog(channel, ...)     KBK_LOG(::KibakoEngine::LogLevel::Info, (channel), __VA_ARGS__)
#define KbkWarn(channel, ...)    KBK_LOG(::KibakoEngine::LogLevel::Warning, (channel), __VA_ARGS__)
#define KbkError(channel, ...)   KBK_LOG(::KibakoEngine::LogLevel::Error, (channel), __VA_ARGS__)
#define KbkCritical(channel, ...)                                                                              \
    KBK_LOG(::KibakoEngine::LogLevel::Critical, (channel), __VA_ARGS__)

#define KbkLogDefault(...)      KbkLog(KBK_LOG_CHANNEL_DEFAULT, __VA_ARGS__)
#define KbkWarnDefault(...)     KbkWarn(KBK_LOG_CHANNEL_DEFAULT, __VA_ARGS__)
#define KbkErrorDefault(...)    KbkError(KBK_LOG_CHANNEL_DEFAULT, __VA_ARGS__)
#define KbkCriticalDefault(...) KbkCritical(KBK_LOG_CHANNEL_DEFAULT, __VA_ARGS__)
