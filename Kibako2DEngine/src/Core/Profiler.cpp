// Collects scoped timing samples and periodically logs summaries
#include "KibakoEngine/Core/Profiler.h"

#if KBK_ENABLE_PROFILING

#include <algorithm>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>

namespace KibakoEngine::Profiler {

    namespace
    {
        using Clock = std::chrono::steady_clock;

        struct SampleData
        {
            double totalMs = 0.0;
            double maxMs = 0.0;
            double minMs = std::numeric_limits<double>::max();
            std::uint32_t hits = 0;
        };

        std::mutex& ProfilerMutex()
        {
            static std::mutex s_mutex;
            return s_mutex;
        }

        std::unordered_map<std::string, SampleData>& Samples()
        {
            static std::unordered_map<std::string, SampleData> s_samples;
            return s_samples;
        }

        std::uint32_t& FrameCounter()
        {
            static std::uint32_t s_frames = 0;
            return s_frames;
        }

        constexpr std::uint32_t kFlushInterval = 120;
    } // namespace

    ScopedEvent::ScopedEvent(const char* name)
        : m_name(name)
        , m_start(Clock::now())
    {
    }

    ScopedEvent::~ScopedEvent()
    {
        const auto end = Clock::now();
        const double ms = std::chrono::duration<double, std::milli>(end - m_start).count();

        std::lock_guard<std::mutex> guard(ProfilerMutex());
        auto& sample = Samples()[m_name ? m_name : "<null>"];
        sample.totalMs += ms;
        sample.maxMs = std::max(sample.maxMs, ms);
        sample.minMs = std::min(sample.minMs, ms);
        sample.hits += 1;
    }

    void BeginFrame()
    {
        bool flushNow = false;
        {
            std::lock_guard<std::mutex> guard(ProfilerMutex());
            auto& frames = FrameCounter();
            ++frames;
            flushNow = (frames % kFlushInterval) == 0;
        }

        if (flushNow)
            Flush();
    }

    void Flush()
    {
        std::unordered_map<std::string, SampleData> snapshot;
        {
            std::lock_guard<std::mutex> guard(ProfilerMutex());
            auto& samples = Samples();
            if (samples.empty())
                return;
            snapshot.swap(samples);
        }

        for (auto& [name, data] : snapshot) {
            if (data.hits == 0)
                continue;
            const double avg = data.totalMs / static_cast<double>(data.hits);
            KbkTrace("Profile", "%s -> avg %.3f ms (min %.3f / max %.3f) across %u samples",
                     name.c_str(), avg, data.minMs, data.maxMs, data.hits);
        }
    }

} // namespace KibakoEngine::Profiler

#endif

