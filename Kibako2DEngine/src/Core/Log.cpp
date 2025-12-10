// Formats and routes log messages to stdout, stderr, and the debugger
#include "KibakoEngine/Core/Log.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>

#include "KibakoEngine/Core/Debug.h"

#ifdef _WIN32
#    include <windows.h>
#endif

namespace KibakoEngine {

    namespace
    {
        constexpr std::size_t kLogBufferSize = 2048;

        std::mutex& OutputMutex()
        {
            static std::mutex s_mutex;
            return s_mutex;
        }

        std::mutex& ConfigMutex()
        {
            static std::mutex s_mutex;
            return s_mutex;
        }

        std::mutex& BreakMessageMutex()
        {
            static std::mutex s_mutex;
            return s_mutex;
        }

        LogConfig& ConfigStorage()
        {
            static LogConfig s_config{};
            return s_config;
        }

        std::atomic<bool>& BreakRequestedFlag()
        {
            static std::atomic<bool> s_requested{false};
            return s_requested;
        }

        std::string& LastBreakMessageStorage()
        {
            static std::string s_message;
            return s_message;
        }

        const char* LevelPrefix(LogLevel level)
        {
            switch (level) {
            case LogLevel::Trace: return "TRACE";
            case LogLevel::Info: return "INFO";
            case LogLevel::Warning: return "WARN";
            case LogLevel::Error: return "ERROR";
            case LogLevel::Critical: return "CRITICAL";
            default: return "LOG";
            }
        }

        void OutputToDebugger(const char* text)
        {
#ifdef _WIN32
            OutputDebugStringA(text);
#else
            KBK_UNUSED(text);
#endif
        }

        bool LocalTime(std::time_t time, std::tm& out)
        {
#if defined(_WIN32)
            return localtime_s(&out, &time) == 0;
#else
            return localtime_r(&time, &out) != nullptr;
#endif
        }

        void StoreBreakpointMessage(const char* message)
        {
            std::string text = message ? message : "";
            while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
                text.pop_back();

            std::lock_guard<std::mutex> guard(BreakMessageMutex());
            LastBreakMessageStorage() = std::move(text);
        }

        void TriggerBreakpoint(LogLevel level, const char* message, const LogConfig& config)
        {
            if (static_cast<int>(level) < static_cast<int>(config.debuggerBreakLevel))
                return;

            if (config.haltRenderingOnBreak)
                BreakRequestedFlag().store(true, std::memory_order_release);

            if (message)
                StoreBreakpointMessage(message);
            else
                StoreBreakpointMessage("");


            if (config.breakIntoDebugger)
                KBK_BREAK();
        }

        LogConfig CopyConfig()
        {
            std::lock_guard<std::mutex> guard(ConfigMutex());
            return ConfigStorage();
        }
    } // namespace

    void SetLogConfig(const LogConfig& config)
    {
        std::lock_guard<std::mutex> guard(ConfigMutex());
        ConfigStorage() = config;
    }

    LogConfig GetLogConfig()
    {
        return CopyConfig();
    }

    void RequestBreakpoint(const char* reason, LogLevel level)
    {
        const LogConfig config = CopyConfig();
        TriggerBreakpoint(level, reason, config);
    }

    bool HasBreakpointRequest()
    {
        return BreakRequestedFlag().load(std::memory_order_acquire);
    }

    bool ConsumeBreakpointRequest()
    {
        return BreakRequestedFlag().exchange(false, std::memory_order_acq_rel);
    }

    const char* LastBreakpointMessage()
    {
        std::lock_guard<std::mutex> guard(BreakMessageMutex());
        return LastBreakMessageStorage().c_str();
    }

    void LogMessageV(LogLevel level,
                     const char* channel,
                     const char* file,
                     int line,
                     const char* function,
                     const char* fmt,
                     std::va_list args)
    {
        const LogConfig config = CopyConfig();
        if (static_cast<int>(level) < static_cast<int>(config.minimumLevel))
            return;

        std::array<char, kLogBufferSize> buffer{};
        auto now = std::chrono::system_clock::now();
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        const auto timeSeconds = static_cast<std::time_t>(ms / 1000);

        std::tm local{};
        LocalTime(timeSeconds, local);

        const auto milliseconds = static_cast<long long>(ms % 1000);

        {
            std::lock_guard<std::mutex> guard(OutputMutex());

            int written = std::snprintf(buffer.data(),
                                        buffer.size(),
                                        "[%02d:%02d:%02d.%03lld][%s]",
                                        local.tm_hour,
                                        local.tm_min,
                                        local.tm_sec,
                                        milliseconds,
                                        LevelPrefix(level));
            if (written < 0)
                return;

            std::size_t offset = static_cast<std::size_t>(written);

            if (channel && channel[0] != '\0') {
                written = std::snprintf(buffer.data() + offset, buffer.size() - offset, "[%s]", channel);
                if (written < 0)
                    return;
                offset += static_cast<std::size_t>(written);
            }

            if (file) {
                const char* filename = std::strrchr(file, '/');
                const char* backslash = std::strrchr(file, '\\');
                const char* trimmed = filename ? filename + 1 : (backslash ? backslash + 1 : file);
                written = std::snprintf(buffer.data() + offset,
                                        buffer.size() - offset,
                                        "[%s:%d]",
                                        trimmed,
                                        line);
                if (written < 0)
                    return;
                offset += static_cast<std::size_t>(written);
            }

            if (function && function[0] != '\0') {
                written = std::snprintf(buffer.data() + offset, buffer.size() - offset, "[%s]", function);
                if (written < 0)
                    return;
                offset += static_cast<std::size_t>(written);
            }

            if (offset < buffer.size()) {
                buffer[offset++] = ' ';
            }

            written = std::vsnprintf(buffer.data() + offset, buffer.size() - offset, fmt, args);
            if (written < 0)
                return;
            offset += static_cast<std::size_t>(written);

            if (offset + 1 < buffer.size())
                buffer[offset++] = '\n';
            buffer[offset] = '\0';

            FILE* stream = (level == LogLevel::Error || level == LogLevel::Critical) ? stderr : stdout;
            std::fputs(buffer.data(), stream);
            std::fflush(stream);
            OutputToDebugger(buffer.data());
        }

        TriggerBreakpoint(level, buffer.data(), config);
    }

    void LogMessage(LogLevel level,
                    const char* channel,
                    const char* file,
                    int line,
                    const char* function,
                    const char* fmt,
                    ...)
    {
        std::va_list args;
        va_start(args, fmt);
        LogMessageV(level, channel, file, line, function, fmt, args);
        va_end(args);
    }

} // namespace KibakoEngine

