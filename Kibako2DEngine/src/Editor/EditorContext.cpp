#include "KibakoEngine/Editor/EditorContext.h"

#include "KibakoEngine/Scene/Scene2D.h"

namespace KibakoEngine {

    void EditorContext::SetActiveScene(Scene2D* scene)
    {
        m_activeScene = scene;

        // If scene changes, selection becomes invalid.
        m_selected = 0;
    }

    void EditorContext::Select(EntityID id)
    {
        m_selected = id;

        // If no active scene, selection is meaningless.
        if (!m_activeScene) {
            m_selected = 0;
            return;
        }

        // If selected entity does not exist, clear selection.
        if (m_selected != 0 && !m_activeScene->FindEntity(m_selected)) {
            m_selected = 0;
        }
    }

    Entity2D* EditorContext::GetSelectedEntity()
    {
        if (!m_activeScene || m_selected == 0)
            return nullptr;
        return m_activeScene->FindEntity(m_selected);
    }

    const Entity2D* EditorContext::GetSelectedEntity() const
    {
        if (!m_activeScene || m_selected == 0)
            return nullptr;
        return m_activeScene->FindEntity(m_selected);
    }

} // namespace KibakoEngine