// Wraps SDL's high-resolution timer for frame timing
#include "KibakoEngine/Core/Time.h"
#include "KibakoEngine/Core/Debug.h"

#include <SDL2/SDL.h>

namespace KibakoEngine {

    void Time::Tick()
    {
        const uint64_t now = SDL_GetPerformanceCounter();

        if (m_invFrequency == 0.0) {
            const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
            KBK_ASSERT(freq > 0.0, "SDL performance counter frequency is zero");
            m_invFrequency = freq > 0.0 ? 1.0 / freq : 0.0;
        }

        if (!m_started) {
            m_started = true;
            m_prevTicks = now;
            m_delta = 0.0;
            m_total = 0.0;

            // FPS init
            m_fpsSmoothed = 0.0;
            m_fpsAccumTime = 0.0;
            m_fpsAccumFrames = 0;
            return;
        }

        m_delta = static_cast<double>(now - m_prevTicks) * m_invFrequency;
        m_prevTicks = now;
        m_total += m_delta;

        // FPS smoothing
        const double dt = (m_delta > 0.0) ? m_delta : 0.0;

        m_fpsAccumTime += dt;
        m_fpsAccumFrames += 1;

        if (m_fpsAccumTime >= m_fpsUpdateInterval) {
            m_fpsSmoothed = (m_fpsAccumTime > 0.0)
                ? (static_cast<double>(m_fpsAccumFrames) / m_fpsAccumTime)
                : 0.0;

            m_fpsAccumTime = 0.0;
            m_fpsAccumFrames = 0;
        }
    }

} // namespace KibakoEngine