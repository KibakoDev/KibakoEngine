// Centralized helpers for assertions and HRESULT validation
#include "KibakoEngine/Core/Debug.h"

#include <cstdio>
#include <cstring>

namespace KibakoEngine::Debug {

    namespace
    {
        const char* ExtractFilename(const char* path)
        {
            if (!path)
                return "<unknown>";
            const char* slash = std::strrchr(path, '\\');
            if (slash)
                return slash + 1;
            slash = std::strrchr(path, '/');
            return slash ? slash + 1 : path;
        }
    } // namespace

    void ReportAssertion(const char* type, const char* condition, const char* message, const char* file, int line)
    {
        const char* filename = ExtractFilename(file);
        KbkCritical("Assert", "%s failed: (%s) -> %s (%s:%d)", type, condition, message ? message : "", filename, line);
    }

    bool VerifyHRESULT(long hr, const char* expression, const char* file, int line)
    {
        if (hr >= 0)
            return true;

        char buffer[16]{};
        std::snprintf(buffer, sizeof(buffer), "0x%08lX", static_cast<unsigned long>(hr));
        ReportAssertion("HRESULT", expression, buffer, file, line);
#if KBK_DEBUG_BUILD
        KBK_BREAK();
#endif
        return false;
    }

} // namespace KibakoEngine::Debug

