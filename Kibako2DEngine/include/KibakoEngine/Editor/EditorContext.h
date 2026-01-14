#pragma once

#include <cstdint>

namespace KibakoEngine {

    class Scene2D;
    struct Entity2D;
    using EntityID = std::uint32_t;

    // Minimal editor state container (Unity-style).
    // - Owns nothing. Just stores pointers/IDs.
    // - Scene lifetime is managed elsewhere (GameLayer / Application).
    class EditorContext
    {
    public:
        EditorContext() = default;

        void SetActiveScene(Scene2D* scene);
        Scene2D* GetActiveScene() { return m_activeScene; }
        const Scene2D* GetActiveScene() const { return m_activeScene; }

        void Select(EntityID id);
        EntityID Selected() const { return m_selected; }

        bool HasSelection() const { return m_selected != 0; }

        Entity2D* GetSelectedEntity();
        const Entity2D* GetSelectedEntity() const;

        void ClearSelection() { m_selected = 0; }

    private:
        Scene2D* m_activeScene = nullptr;
        EntityID  m_selected = 0;
    };

} // namespace KibakoEngine