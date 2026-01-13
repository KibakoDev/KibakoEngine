#pragma once

#include <cstdint>

namespace KibakoEngine {

    class Scene2D;

    class EditorContext
    {
    public:
        // Scene
        void SetActiveScene(Scene2D* scene) { m_activeScene = scene; }
        [[nodiscard]] Scene2D* GetActiveScene() const { return m_activeScene; }

        // Selection
        void Select(std::uint32_t id) { m_selected = id; }
        [[nodiscard]] std::uint32_t Selected() const { return m_selected; }

    private:
        Scene2D* m_activeScene = nullptr;
        std::uint32_t  m_selected = 0;
    };

} // namespace KibakoEngine