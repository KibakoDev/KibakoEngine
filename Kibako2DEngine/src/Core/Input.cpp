// Tracks keyboard and mouse input across frames using SDL events
#include "KibakoEngine/Core/Input.h"

#include <algorithm>
#include <cstring>

namespace KibakoEngine {

    Input::Input()
    {
        m_keyboard = nullptr;
        std::fill(m_prevKeyboard.begin(), m_prevKeyboard.end(), static_cast<uint8_t>(0));
    }

    void Input::BeginFrame()
    {
        m_wheelX = 0;
        m_wheelY = 0;
        m_textChar = 0;
        m_prevMouseButtons = m_mouseButtons;
        m_keyboard = SDL_GetKeyboardState(nullptr);
    }

    void Input::HandleEvent(const SDL_Event& e)
    {
        switch (e.type) {
        case SDL_MOUSEMOTION:
            m_mouseX = e.motion.x;
            m_mouseY = e.motion.y;
            break;
        case SDL_MOUSEBUTTONDOWN:
            m_mouseButtons |= SDL_BUTTON(e.button.button);
            break;
        case SDL_MOUSEBUTTONUP:
            m_mouseButtons &= ~SDL_BUTTON(e.button.button);
            break;
        case SDL_MOUSEWHEEL:
            m_wheelX += e.wheel.x;
            m_wheelY += e.wheel.y;
            break;
        case SDL_TEXTINPUT:
            if (const unsigned char c = static_cast<unsigned char>(e.text.text[0]); c >= 32u && c < 127u) {
                m_textChar = c;
            }
            break;
        default:
            break;
        }
    }

    void Input::EndFrame()
    {
        if (m_keyboard) {
            std::memcpy(m_prevKeyboard.data(), m_keyboard, m_prevKeyboard.size());
        }
    }

    bool Input::KeyDown(SDL_Scancode scancode) const
    {
        return m_keyboard && m_keyboard[scancode] != 0;
    }

    bool Input::KeyPressed(SDL_Scancode scancode) const
    {
        const uint8_t now = m_keyboard ? m_keyboard[scancode] : 0;
        const uint8_t prev = m_prevKeyboard[scancode];
        return (now != 0u) && (prev == 0u);
    }

    bool Input::MouseDown(uint8_t button) const
    {
        return (m_mouseButtons & SDL_BUTTON(button)) != 0u;
    }

    bool Input::MousePressed(uint8_t button) const
    {
        const uint32_t mask = SDL_BUTTON(button);
        return ((m_mouseButtons & mask) != 0u) && ((m_prevMouseButtons & mask) == 0u);
    }

} // namespace KibakoEngine

