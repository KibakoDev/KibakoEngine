// Tracks high-resolution timing information between frames
#pragma once

#include <cstdint>

namespace KibakoEngine {

    class Time {
    public:
        void Tick();

        [[nodiscard]] double DeltaSeconds() const { return m_delta; }
        [[nodiscard]] double TotalSeconds() const { return m_total; }

        // Instant FPS
        [[nodiscard]] double FPSInstant() const { return m_delta > 0.0 ? 1.0 / m_delta : 0.0; }

        // Smoothed FPS
        [[nodiscard]] double FPS() const { return m_fpsSmoothed; }

    private:
        // Core timing
        double   m_delta = 0.0;
        double   m_total = 0.0;
        uint64_t m_prevTicks = 0;
        double   m_invFrequency = 0.0;
        bool     m_started = false;

        // FPS smoothing
        double   m_fpsSmoothed = 0.0;
        double   m_fpsAccumTime = 0.0;
        uint32_t m_fpsAccumFrames = 0;
        double   m_fpsUpdateInterval = 0.25;
    };

} // namespace KibakoEngine