// Tracks keyboard and mouse state on a per-frame basis
#pragma once

#include <SDL2/SDL.h>
#include <cstdint>
#include <array>
#include <string>
#include <unordered_map>
#include <vector>

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

        // ------------------------------------------------------------
        // Input Actions (Phase: ergonomics)
        // ------------------------------------------------------------

        // Adds a binding. Supports multiple bindings per action.
        void BindAction(const std::string& action, SDL_Scancode scancode);
        void ClearActionBindings(const std::string& action);
        void ClearAllActionBindings();

        [[nodiscard]] bool ActionDown(const std::string& action) const;
        [[nodiscard]] bool ActionPressed(const std::string& action) const;
        [[nodiscard]] bool ActionReleased(const std::string& action) const;

        // Convenience: axis made of two actions (e.g. Up/Down or Left/Right)
        // Returns -1..+1
        [[nodiscard]] float ActionAxis1D(const std::string& negativeAction,
            const std::string& positiveAction) const;

    private:
        // --- Low-level state
        const uint8_t* m_keyboard = nullptr;
        std::array<uint8_t, SDL_NUM_SCANCODES> m_prevKeyboard{};

        int      m_mouseX = 0;
        int      m_mouseY = 0;
        int      m_wheelX = 0;
        int      m_wheelY = 0;
        uint32_t m_mouseButtons = 0;
        uint32_t m_prevMouseButtons = 0;

        uint32_t m_textChar = 0;

        // --- Actions
        struct ActionState
        {
            std::vector<SDL_Scancode> bindings;
            bool down = false;
            bool pressed = false;
            bool released = false;
        };

        std::unordered_map<std::string, ActionState> m_actions;

        void UpdateActions();
    };

} // namespace KibakoEngine