// Connects RmlUI to platform services such as time, logging, and cursors
#pragma once

#include <RmlUi/Core/SystemInterface.h>
#include <chrono>

// Avoid including SDL headers here; forward declare the cursor type instead
struct SDL_Cursor;

namespace KibakoEngine {

    class RmlSystemInterface : public Rml::SystemInterface {
    public:
        RmlSystemInterface();
        ~RmlSystemInterface() override;

        // Engine time in seconds since startup
        double GetElapsedTime() override;

        // Clipboard helpers
        void SetClipboardText(const Rml::String& text) override;
        void GetClipboardText(Rml::String& text) override;

        // Route RmlUI logs to the engine logger
        bool LogMessage(Rml::Log::Type type, const Rml::String& message) override;

        // Update the mouse cursor when the CSS `cursor` property changes
        void SetMouseCursor(const Rml::String& cursor_name) override;

    private:
        std::chrono::steady_clock::time_point m_startTime;

        // Cached SDL cursors reused by RmlUI
        SDL_Cursor* m_cursorArrow = nullptr;
        SDL_Cursor* m_cursorHand = nullptr;
    };

} // namespace KibakoEngine
