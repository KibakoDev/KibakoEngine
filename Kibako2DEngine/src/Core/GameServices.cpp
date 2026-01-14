// Maintains global gameplay states
#include "KibakoEngine/Core/GameServices.h"

#include "KibakoEngine/Core/Log.h"

namespace KibakoEngine::GameServices {

    namespace
    {
        constexpr const char* kLogChannel = "GameServices";

        GameTime g_time{};
        bool     g_initialized = false;

        void EnsureInitialized()
        {
            if (!g_initialized)
                Init();
        }
    }

    void Init()
    {
        g_time = GameTime{};
        g_time.timeScale = 1.0;
        g_time.paused = false;
        g_initialized = true;

        KbkLog(kLogChannel, "GameServices initialized");
    }

    void Shutdown()
    {
        if (!g_initialized)
            return;

        g_time = GameTime{};
        g_initialized = false;

        KbkLog(kLogChannel, "GameServices shutdown");
    }

    void Update(double rawDeltaSeconds)
    {
        EnsureInitialized();

        if (rawDeltaSeconds < 0.0)
            rawDeltaSeconds = 0.0;

        g_time.rawDeltaSeconds = rawDeltaSeconds;
        g_time.totalRawSeconds += rawDeltaSeconds;

        if (g_time.paused || g_time.timeScale <= 0.0) {
            g_time.scaledDeltaSeconds = 0.0;
            return;
        }

        const double scaledDt = rawDeltaSeconds * g_time.timeScale;
        g_time.scaledDeltaSeconds = scaledDt;
        g_time.totalScaledSeconds += scaledDt;
    }

    const GameTime& GetTime()
    {
        EnsureInitialized();
        return g_time;
    }

    double GetScaledDeltaTime()
    {
        return GetTime().scaledDeltaSeconds;
    }

    double GetRawDeltaTime()
    {
        return GetTime().rawDeltaSeconds;
    }

    void SetTimeScale(double scale)
    {
        EnsureInitialized();

        if (scale < 0.0)
            scale = 0.0;
        g_time.timeScale = scale;
    }

    double GetTimeScale()
    {
        return GetTime().timeScale;
    }

    void SetPaused(bool paused)
    {
        EnsureInitialized();
        g_time.paused = paused;
    }

    bool IsPaused()
    {
        return GetTime().paused;
    }

    void TogglePause()
    {
        EnsureInitialized();
        g_time.paused = !g_time.paused;
    }

} // namespace KibakoEngine::GameServices
