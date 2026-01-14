// Tracks keyboard/mouse input across frames using SDL + Input Actions
#include "KibakoEngine/Core/Input.h"

#include <algorithm>
#include <cstring>

namespace KibakoEngine {

    Input::Input()
    {
        std::fill(m_prevKeyboard.begin(), m_prevKeyboard.end(), static_cast<uint8_t>(0));
        m_keyboard = nullptr;
    }

    void Input::BeginFrame()
    {
        m_wheelX = 0;
        m_wheelY = 0;
        m_textChar = 0;

        m_prevMouseButtons = m_mouseButtons;

        // Snapshot pointer for this frame (SDL updates this state when events are pumped)
        m_keyboard = SDL_GetKeyboardState(nullptr);

        // IMPORTANT:
        // Don't UpdateActions() here. It must happen AFTER SDL events are pumped.
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
            // First ASCII printable char (simple + deterministic for now)
            if (const unsigned char c = static_cast<unsigned char>(e.text.text[0]);
                c >= 32u && c < 127u) {
                m_textChar = c;
            }
            break;

        default:
            break;
        }
    }

    void Input::AfterEvents()
    {
        // Now SDL has processed the events, so keyboard state is up-to-date
        UpdateActions();
    }

    void Input::EndFrame()
    {
        // Store current keyboard snapshot as previous for next frame
        if (m_keyboard) {
            std::memcpy(m_prevKeyboard.data(), m_keyboard, m_prevKeyboard.size());
        }
    }

    // ------------------------------------------------------------
    // Keyboard
    // ------------------------------------------------------------

    bool Input::KeyDown(SDL_Scancode scancode) const
    {
        return m_keyboard && (m_keyboard[scancode] != 0);
    }

    bool Input::KeyPressed(SDL_Scancode scancode) const
    {
        const uint8_t now = m_keyboard ? m_keyboard[scancode] : 0;
        const uint8_t prev = m_prevKeyboard[scancode];
        return (now != 0u) && (prev == 0u);
    }

    bool Input::KeyReleased(SDL_Scancode scancode) const
    {
        const uint8_t now = m_keyboard ? m_keyboard[scancode] : 0;
        const uint8_t prev = m_prevKeyboard[scancode];
        return (now == 0u) && (prev != 0u);
    }

    // ------------------------------------------------------------
    // Mouse
    // ------------------------------------------------------------

    bool Input::MouseDown(uint8_t button) const
    {
        return (m_mouseButtons & SDL_BUTTON(button)) != 0u;
    }

    bool Input::MousePressed(uint8_t button) const
    {
        const uint32_t mask = SDL_BUTTON(button);
        return ((m_mouseButtons & mask) != 0u) && ((m_prevMouseButtons & mask) == 0u);
    }

    bool Input::MouseReleased(uint8_t button) const
    {
        const uint32_t mask = SDL_BUTTON(button);
        return ((m_mouseButtons & mask) == 0u) && ((m_prevMouseButtons & mask) != 0u);
    }

    // ------------------------------------------------------------
    // Actions
    // ------------------------------------------------------------

    void Input::BindAction(std::string action, SDL_Scancode scancode)
    {
        auto& st = m_actions[std::move(action)];

        // Avoid duplicates
        if (std::find(st.bindings.begin(), st.bindings.end(), scancode) == st.bindings.end()) {
            st.bindings.push_back(scancode);
        }
    }

    void Input::ClearActionBindings(std::string_view action)
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

    bool Input::ActionDown(std::string_view action) const
    {
        auto it = m_actions.find(action);
        return (it != m_actions.end()) ? it->second.down : false;
    }

    bool Input::ActionPressed(std::string_view action) const
    {
        auto it = m_actions.find(action);
        return (it != m_actions.end()) ? it->second.pressed : false;
    }

    bool Input::ActionReleased(std::string_view action) const
    {
        auto it = m_actions.find(action);
        return (it != m_actions.end()) ? it->second.released : false;
    }

    float Input::ActionAxis1D(std::string_view negativeAction,
        std::string_view positiveAction) const
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
        for (auto& entry : m_actions) {
            auto& st = entry.second;
            bool nowDown = false;
            bool prevDown = false;

            for (SDL_Scancode sc : st.bindings) {
                const bool n = (m_keyboard && (m_keyboard[sc] != 0));
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