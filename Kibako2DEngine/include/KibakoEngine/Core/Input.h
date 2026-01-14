// Tracks keyboard and mouse state on a per-frame basis + Input Actions
#pragma once

#include <SDL2/SDL.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace KibakoEngine {

    class Input
    {
    public:
        Input();

        // Called at the beginning of the frame (before event pumping)
        void BeginFrame();

        // Called for each SDL event
        void HandleEvent(const SDL_Event& e);

        // Called once after all events are pumped (IMPORTANT for action states)
        void AfterEvents();

        // Called at the end of the frame (after rendering)
        void EndFrame();

        // ------------------------------------------------------------
        // Low-level keyboard
        // ------------------------------------------------------------
        [[nodiscard]] bool KeyDown(SDL_Scancode scancode) const;
        [[nodiscard]] bool KeyPressed(SDL_Scancode scancode) const;
        [[nodiscard]] bool KeyReleased(SDL_Scancode scancode) const;

        // ------------------------------------------------------------
        // Mouse
        // ------------------------------------------------------------
        [[nodiscard]] bool MouseDown(uint8_t button) const;
        [[nodiscard]] bool MousePressed(uint8_t button) const;
        [[nodiscard]] bool MouseReleased(uint8_t button) const;

        [[nodiscard]] int  MouseX() const { return m_mouseX; }
        [[nodiscard]] int  MouseY() const { return m_mouseY; }
        [[nodiscard]] int  WheelX() const { return m_wheelX; }
        [[nodiscard]] int  WheelY() const { return m_wheelY; }

        // Single ASCII char (32..126) captured this frame, 0 if none
        [[nodiscard]] uint32_t TextChar() const { return m_textChar; }

        // ------------------------------------------------------------
        // Input Actions (ergonomic layer)
        // ------------------------------------------------------------
        void BindAction(std::string action, SDL_Scancode scancode);
        void ClearActionBindings(std::string_view action);
        void ClearAllActionBindings();

        [[nodiscard]] bool ActionDown(std::string_view action) const;
        [[nodiscard]] bool ActionPressed(std::string_view action) const;
        [[nodiscard]] bool ActionReleased(std::string_view action) const;

        // Returns -1..+1 (e.g. Left/Right)
        [[nodiscard]] float ActionAxis1D(std::string_view negativeAction,
            std::string_view positiveAction) const;

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

        // Transparent hash so we can query with std::string_view without allocating
        struct TransparentHash
        {
            using is_transparent = void;
            size_t operator()(std::string_view sv) const noexcept
            {
                return std::hash<std::string_view>{}(sv);
            }
            size_t operator()(const std::string& s) const noexcept
            {
                return std::hash<std::string_view>{}(s);
            }
        };

        struct TransparentEq
        {
            using is_transparent = void;
            bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
            bool operator()(const std::string& a, std::string_view b) const noexcept { return a == b; }
            bool operator()(std::string_view a, const std::string& b) const noexcept { return a == b; }
        };

        std::unordered_map<std::string, ActionState, TransparentHash, TransparentEq> m_actions;

    private:
        void UpdateActions();
    };

} // namespace KibakoEngine