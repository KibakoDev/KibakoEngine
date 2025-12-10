// Tracks keyboard and mouse state on a per-frame basis
#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <array>

namespace KibakoEngine {

    class Input {
    public:
        Input();

        void BeginFrame();
        void HandleEvent(const SDL_Event& e);
        void EndFrame();

        [[nodiscard]] bool KeyDown(SDL_Scancode scancode) const;
        [[nodiscard]] bool KeyPressed(SDL_Scancode scancode) const;

        [[nodiscard]] bool MouseDown(uint8_t button) const;
        [[nodiscard]] bool MousePressed(uint8_t button) const;

        [[nodiscard]] int  MouseX() const { return m_mouseX; }
        [[nodiscard]] int  MouseY() const { return m_mouseY; }
        [[nodiscard]] int  WheelX() const { return m_wheelX; }
        [[nodiscard]] int  WheelY() const { return m_wheelY; }

        [[nodiscard]] uint32_t TextChar() const { return m_textChar; }

    private:
        const uint8_t* m_keyboard = nullptr;
        std::array<uint8_t, SDL_NUM_SCANCODES> m_prevKeyboard{};

        int      m_mouseX = 0;
        int      m_mouseY = 0;
        int      m_wheelX = 0;
        int      m_wheelY = 0;
        uint32_t m_mouseButtons = 0;
        uint32_t m_prevMouseButtons = 0;

        uint32_t m_textChar = 0;
    };

} // namespace KibakoEngine

