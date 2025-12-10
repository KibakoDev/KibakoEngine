// Small helpers for tracking elapsed and countdown time
#pragma once

#include <algorithm>

namespace KibakoEngine::Gameplay {

    // Stopwatch that keeps increasing while it is running
    class Stopwatch
    {
    public:
        Stopwatch() = default;

        void Start()
        {
            m_running = true;
        }

        void Stop()
        {
            m_running = false;
        }

        void Reset()
        {
            m_time = 0.0f;
        }

        void Restart()
        {
            m_time = 0.0f;
            m_running = true;
        }

        void Update(float dt)
        {
            if (!m_running)
                return;

            m_time += dt;
        }

        [[nodiscard]] float GetTime() const
        {
            return m_time;
        }

        [[nodiscard]] bool IsRunning() const
        {
            return m_running;
        }

    private:
        float m_time = 0.0f;
        bool  m_running = false;
    };

    // Countdown timer that reaches zero then stops
    class CountdownTimer
    {
    public:
        CountdownTimer() = default;

        explicit CountdownTimer(float durationSeconds)
            : m_duration(durationSeconds)
            , m_remaining(durationSeconds)
        {
        }

        void SetDuration(float durationSeconds)
        {
            m_duration = durationSeconds;
            m_remaining = std::min(m_remaining, m_duration);
        }

        [[nodiscard]] float GetDuration() const
        {
            return m_duration;
        }

        void Reset()
        {
            m_remaining = 0.0f;
            m_running = false;
            m_finished = false;
        }

        void Restart()
        {
            m_remaining = m_duration;
            m_running = m_duration > 0.0f;
            m_finished = false;
        }

        void Start()
        {
            if (m_duration <= 0.0f)
                return;

            if (m_remaining <= 0.0f)
                m_remaining = m_duration;

            m_running = true;
            m_finished = false;
        }

        void Stop()
        {
            m_running = false;
        }

        void Update(float dt)
        {
            if (!m_running || m_finished || m_duration <= 0.0f)
                return;

            m_remaining -= dt;
            if (m_remaining <= 0.0f) {
                m_remaining = 0.0f;
                m_running = false;
                m_finished = true;
            }
        }

        [[nodiscard]] bool IsFinished() const
        {
            return m_finished;
        }

        [[nodiscard]] bool IsRunning() const
        {
            return m_running;
        }

        [[nodiscard]] float GetRemainingTime() const
        {
            return m_remaining;
        }

        [[nodiscard]] float GetProgress01() const
        {
            if (m_duration <= 0.0f)
                return 1.0f;

            const float progress = 1.0f - (m_remaining / m_duration);
            return std::clamp(progress, 0.0f, 1.0f);
        }

    private:
        float m_duration = 0.0f;   // total duration in seconds
        float m_remaining = 0.0f;  // time remaining in seconds
        bool  m_running = false;
        bool  m_finished = false;
    };

} // namespace KibakoEngine::Gameplay

