// Global services used by gameplay systems (time scaling and pause)
#pragma once

#include <cstdint>

namespace KibakoEngine {

    struct GameTime
    {
        double rawDeltaSeconds = 0.0;
        double scaledDeltaSeconds = 0.0;

        double totalRawSeconds = 0.0;
        double totalScaledSeconds = 0.0;

        double timeScale = 1.0;
        bool   paused = false;
    };

    namespace GameServices
    {
        void Init();
        void Shutdown();

        void Update(double rawDeltaSeconds);

        [[nodiscard]] const GameTime& GetTime();

        [[nodiscard]] double GetScaledDeltaTime();
        [[nodiscard]] double GetRawDeltaTime();

        void   SetTimeScale(double scale);
        [[nodiscard]] double GetTimeScale();

        void SetPaused(bool paused);
        [[nodiscard]] bool IsPaused();
        void TogglePause();
    }

} // namespace KibakoEngine
