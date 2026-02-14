// KibakoEngine/UI/EditorOverlay.h
#pragma once

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Scene/ComponentStore.h"

#if KBK_DEBUG_BUILD

#include <cstdint>
#include <functional>
#include <string>

namespace Rml {
    class ElementDocument;
    class Element;
    class ElementFormControlInput;
    class Event;
}

namespace KibakoEngine {

    class Application;
    class Scene2D;

    // Engine Debug Editor Overlay (Debug builds only)
    class EditorOverlay
    {
    public:
        void Init(Application& app);
        void Shutdown();

        void SetScene(Scene2D* scene);

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }

        // Optional hooks for UI buttons
        void SetOnApply(std::function<void()> fn) { m_onApply = std::move(fn); }

        void Update(float dt);

    private:
        void BindButtons();
        void SelectEntity(EntityID id);
        void OnQuitClicked(Rml::Event& e);
        void OnApplyClicked(Rml::Event& e);
        void OnHierarchyEntityClicked(EntityID id, Rml::Event& e);
        void OnInspectorFocus(Rml::Event& e);
        void OnInspectorBlur(Rml::Event& e);
        void OnInspectorInputChanged(Rml::Event& e);

        void MarkHierarchyDirty();
        void MarkInspectorDirty();
        void MarkStatsDirty();

        void RefreshStats();
        void RefreshHierarchy();
        void RefreshInspector(bool force = false);
        void ApplyInspector();

        [[nodiscard]] std::uint64_t BuildSceneDigest() const;

    private:
        Application* m_app = nullptr;
        Scene2D* m_scene = nullptr;

        Rml::ElementDocument* m_doc = nullptr;
        Rml::Element* m_statsEntities = nullptr;
        Rml::Element* m_statsFps = nullptr;
        Rml::Element* m_hierarchyList = nullptr;
        Rml::Element* m_inspectorHint = nullptr;
        Rml::ElementFormControlInput* m_insName = nullptr;
        Rml::ElementFormControlInput* m_insPosX = nullptr;
        Rml::ElementFormControlInput* m_insPosY = nullptr;
        Rml::ElementFormControlInput* m_insRot = nullptr;
        Rml::ElementFormControlInput* m_insScaleX = nullptr;
        Rml::ElementFormControlInput* m_insScaleY = nullptr;

        EntityID m_selectedEntity = 0;

        std::function<void()> m_onApply;

        float m_statsAccum = 0.0f;
        float m_statsPeriod = 0.10f; // 10x/sec
        float m_refreshAccum = 0.0f;
        float m_refreshPeriod = 0.10f; // 10x/sec polling for scene changes

        bool m_rebuildHierarchy = true;
        bool m_rebuildInspector = true;
        bool m_refreshStats = true;
        bool m_inspectorEditing = false;

        std::uint64_t m_lastSceneDigest = 0;

        bool  m_enabled = true;
    };

} // namespace KibakoEngine

#else

// Release build: compiled out
namespace KibakoEngine { class EditorOverlay {}; }

#endif // KBK_DEBUG_BUILD
