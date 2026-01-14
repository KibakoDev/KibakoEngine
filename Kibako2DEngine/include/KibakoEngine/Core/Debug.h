// Helpers for assertions, verification, and diagnostic logging
#pragma once

#include "KibakoEngine/Core/Log.h"

namespace KibakoEngine::Debug {

    void ReportAssertion(const char* type, const char* condition, const char* message, const char* file, int line);
    void ReportVerification(const char* condition, const char* message, const char* file, int line);
    bool VerifyHRESULT(long hr, const char* expression, const char* file, int line);

} // namespace KibakoEngine::Debug

#if !defined(KBK_DEBUG_BUILD)
#    if defined(_DEBUG) || !defined(NDEBUG)
#        define KBK_DEBUG_BUILD 1
#    else
#        define KBK_DEBUG_BUILD 0
#    endif
#endif

#if !defined(KBK_BREAK)
#    if defined(_MSC_VER)
#        include <intrin.h>
#        define KBK_BREAK() __debugbreak()
#    else
#        define KBK_BREAK() ((void)0)
#    endif
#endif

#define KBK_TRIGGER_BREAK(reason) ::KibakoEngine::RequestBreakpoint((reason), ::KibakoEngine::LogLevel::Critical)

#define KBK_UNUSED(x) ((void)(x))

#if KBK_DEBUG_BUILD
#    define KBK_ASSERT(condition, message)                                                                   \
        do {                                                                                                 \
            if (!(condition)) {                                                                              \
                ::KibakoEngine::Debug::ReportAssertion("ASSERT", #condition, (message), __FILE__, __LINE__); \
                KBK_BREAK();                                                                                  \
            }                                                                                                \
        } while (0)
#else
#    define KBK_ASSERT(condition, message) ((void)sizeof(condition))
#endif

#if KBK_DEBUG_BUILD
#    define KBK_VERIFY(condition, message) KBK_ASSERT((condition), (message))
#else
#    define KBK_VERIFY(condition, message)                                                          \
        do {                                                                                        \
            if (!(condition)) {                                                                     \
                ::KibakoEngine::Debug::ReportVerification(#condition, (message), __FILE__, __LINE__); \
            }                                                                                       \
        } while (0)
#endif

#define KBK_HR(expression) \
    ::KibakoEngine::Debug::VerifyHRESULT( \
        static_cast<long>(expression), #expression, __FILE__, __LINE__)
