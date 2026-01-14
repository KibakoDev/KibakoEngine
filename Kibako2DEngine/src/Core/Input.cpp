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

        // Snapshot pointer for this frame
        m_keyboard = SDL_GetKeyboardState(nullptr);

        // Actions get updated per-frame using current & previous keyboard snapshots
        UpdateActions();
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

    // ------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------

    void Input::BindAction(const std::string& action, SDL_Scancode scancode)
    {
        auto& st = m_actions[action];

        // Avoid duplicates
        if (std::find(st.bindings.begin(), st.bindings.end(), scancode) == st.bindings.end()) {
            st.bindings.push_back(scancode);
        }
    }

    void Input::ClearActionBindings(const std::string& action)
    {
        auto it = m_actions.find(action);
        if (it == m_actions.end())
            return;

        it->second.bindings.clear();
        it->second.down = false;
        it->second.pressed = false;
        it->second.released = false;
    }

    void Input::ClearAllActionBindings()
    {
        m_actions.clear();
    }

    bool Input::ActionDown(const std::string& action) const
    {
        auto it = m_actions.find(action);
        if (it == m_actions.end())
            return false;
        return it->second.down;
    }

    bool Input::ActionPressed(const std::string& action) const
    {
        auto it = m_actions.find(action);
        if (it == m_actions.end())
            return false;
        return it->second.pressed;
    }

    bool Input::ActionReleased(const std::string& action) const
    {
        auto it = m_actions.find(action);
        if (it == m_actions.end())
            return false;
        return it->second.released;
    }

    float Input::ActionAxis1D(const std::string& negativeAction,
        const std::string& positiveAction) const
    {
        const bool neg = ActionDown(negativeAction);
        const bool pos = ActionDown(positiveAction);

        if (neg == pos)
            return 0.0f;
        return pos ? 1.0f : -1.0f;
    }

    void Input::UpdateActions()
    {
        // Compute action state from current/previous keyboard snapshots
        for (auto& [name, st] : m_actions) {
            bool nowDown = false;
            bool prevDown = false;

            for (SDL_Scancode sc : st.bindings) {
                const bool n = (m_keyboard && m_keyboard[sc] != 0);
                const bool p = (m_prevKeyboard[sc] != 0);
                nowDown |= n;
                prevDown |= p;
            }

            st.down = nowDown;
            st.pressed = (nowDown && !prevDown);
            st.released = (!nowDown && prevDown);
        }
    }

} // namespace KibakoEngine