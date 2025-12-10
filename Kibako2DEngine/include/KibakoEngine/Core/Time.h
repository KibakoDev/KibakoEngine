// Tracks high-resolution timing information between frames
#pragma once

#include <cstdint>

namespace KibakoEngine {

    class Time {
    public:
        void Tick();

        [[nodiscard]] double DeltaSeconds() const { return m_delta; }
        [[nodiscard]] double TotalSeconds() const { return m_total; }
        [[nodiscard]] double FPS() const { return m_delta > 0.0 ? 1.0 / m_delta : 0.0; }

    private:
        double   m_delta = 0.0;
        double   m_total = 0.0;
        uint64_t m_prevTicks = 0;
        double   m_invFrequency = 0.0;
        bool     m_started = false;
    };

} // namespace KibakoEngine

