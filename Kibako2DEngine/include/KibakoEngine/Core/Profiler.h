// Small RAII helpers for lightweight profiling markers
#pragma once

#include <cstdint>
#include <chrono>

#include "KibakoEngine/Core/Debug.h"

#if !defined(KBK_ENABLE_PROFILING)
#    if KBK_DEBUG_BUILD
#        define KBK_ENABLE_PROFILING 1
#    else
#        define KBK_ENABLE_PROFILING 0
#    endif
#endif

namespace KibakoEngine::Profiler {

#if KBK_ENABLE_PROFILING

    void BeginFrame();
    void Flush();

    class ScopedEvent
    {
    public:
        explicit ScopedEvent(const char* name);
        ~ScopedEvent();

        ScopedEvent(const ScopedEvent&) = delete;
        ScopedEvent& operator=(const ScopedEvent&) = delete;

    private:
        const char* m_name = nullptr;
        std::chrono::steady_clock::time_point m_start;
    };

#else

    inline void BeginFrame() {}
    inline void Flush() {}

    class ScopedEvent
    {
    public:
        explicit ScopedEvent(const char*) {}
    };

#endif

} // namespace KibakoEngine::Profiler

#define KBK_CONCAT_INNER(a, b) a##b
#define KBK_CONCAT(a, b) KBK_CONCAT_INNER(a, b)

#if KBK_ENABLE_PROFILING
#    define KBK_PROFILE_SCOPE(name) ::KibakoEngine::Profiler::ScopedEvent KBK_CONCAT(_kbkProfileScope, __LINE__)(name)
#else
#    define KBK_PROFILE_SCOPE(name) ((void)0)
#endif

#define KBK_PROFILE_FUNCTION() KBK_PROFILE_SCOPE(__FUNCTION__)
#define KBK_PROFILE_FRAME(name) KBK_PROFILE_SCOPE(name)

