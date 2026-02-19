// Implements the RmlUI system interface using SDL utilities
#include "KibakoEngine/UI/RmlSystemInterface.h"
#include "KibakoEngine/Core/Log.h"

#include <SDL2/SDL.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "RmlSystem";
    }

    RmlSystemInterface::RmlSystemInterface()
    {
        m_startTime = std::chrono::steady_clock::now();

        // Create the system cursors used by the UI
        m_cursorArrow = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
        m_cursorHand = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
        m_cursorText = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);

        // Use the default arrow cursor initially
        if (m_cursorArrow)
            SDL_SetCursor(m_cursorArrow);
    }

    RmlSystemInterface::~RmlSystemInterface()
    {
        if (m_cursorHand)
            SDL_FreeCursor(m_cursorHand);
        if (m_cursorText)
            SDL_FreeCursor(m_cursorText);
        if (m_cursorArrow)
            SDL_FreeCursor(m_cursorArrow);
    }

    double RmlSystemInterface::GetElapsedTime()
    {
        std::chrono::duration<double> secs =
            std::chrono::steady_clock::now() - m_startTime;
        return secs.count();
    }

    void RmlSystemInterface::SetClipboardText(const Rml::String& text)
    {
        if (SDL_SetClipboardText(text.c_str()) != 0) {
            KbkWarn(kLogChannel, "SDL_SetClipboardText failed: %s", SDL_GetError());
        }
    }

    void RmlSystemInterface::GetClipboardText(Rml::String& text)
    {
        char* clipboard = SDL_GetClipboardText();
        if (!clipboard) {
            KbkWarn(kLogChannel, "SDL_GetClipboardText failed: %s", SDL_GetError());
            text.clear();
            return;
        }

        text = clipboard;
        SDL_free(clipboard);
    }

    void RmlSystemInterface::SetMouseCursor(const Rml::String& cursor_name)
    {
        // RmlUi passes the raw CSS cursor property value here
        SDL_Cursor* cursor = m_cursorArrow;

        if (cursor_name == "pointer")
            cursor = m_cursorHand;
        else if (cursor_name == "text")
            cursor = m_cursorText;
        else if (cursor_name.empty())
            cursor = m_cursorArrow;

        if (cursor)
            SDL_SetCursor(cursor);
    }

    bool RmlSystemInterface::LogMessage(Rml::Log::Type type, const Rml::String& message)
    {
        switch (type) {
        case Rml::Log::LT_ERROR:
        case Rml::Log::LT_ASSERT:
            KbkError(kLogChannel, "%s", message.c_str());
            break;
        case Rml::Log::LT_WARNING:
            KbkWarn(kLogChannel, "%s", message.c_str());
            break;
        case Rml::Log::LT_INFO:
        case Rml::Log::LT_DEBUG:
        default:
            KbkLog(kLogChannel, "%s", message.c_str());
            break;
        }
        return true;
    }

} // namespace KibakoEngine