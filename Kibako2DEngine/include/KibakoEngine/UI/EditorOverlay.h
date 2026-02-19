// KibakoEngine/UI/EditorOverlay.h
#pragma once

#include "KibakoEngine/Core/Debug.h"

#if KBK_DEBUG_BUILD

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Rml {
    class Element;
    class ElementDocument;
    class ElementFormControlInput;
}

namespace KibakoEngine {

    class Application;
    class Scene2D;

    using EntityID = std::uint32_t;

    // Debug-only editor overlay powered by RmlUI.
    // V0 goal: stable, minimal, no DOM churn.
    class EditorOverlay final
    {
    public:
        void Init(Application& app);
        void Shutdown();

        void SetScene(Scene2D* scene);

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }

        void SetOnApply(std::function<void()> fn) { m_onApply = std::move(fn); }

        void Update(float dt);

    private:
        // UI wiring
        void BindTopbarButtons();
        void BindTreePolicyWheelOnly(); // blocks LMB drag-scroll on tree panel background

        // Tree
        void MarkTreeDirty();
        void RefreshTreeIfNeeded();
        void RebuildTreeFlatList();

        // Selection / inspector
        void SelectEntity(EntityID id);
        void RefreshStats();
        void RefreshInspector();
        void ApplyInspector();

        bool HasFocusedInspectorField() const;
        void SetInspectorDefaultValues();

    private:
        Application* m_app = nullptr;
        Scene2D* m_scene = nullptr;

        bool m_enabled = true;

        // Document + cached elements
        Rml::ElementDocument* m_doc = nullptr;

        Rml::Element* m_statsEntities = nullptr;
        Rml::Element* m_statsFps = nullptr;

        Rml::Element* m_treeList = nullptr;

        Rml::Element* m_inspectorHint = nullptr;
        Rml::ElementFormControlInput* m_insName = nullptr;
        Rml::ElementFormControlInput* m_insPosX = nullptr;
        Rml::ElementFormControlInput* m_insPosY = nullptr;
        Rml::ElementFormControlInput* m_insRot = nullptr;
        Rml::ElementFormControlInput* m_insScaleX = nullptr;
        Rml::ElementFormControlInput* m_insScaleY = nullptr;

        // State
        EntityID m_selectedEntity = 0;
        bool     m_isApplyingInspector = false;

        // Dirty flags
        bool m_treeDirty = true;
        bool m_inspectorDirty = true;
        bool m_statsDirty = true;

        // Minimal “structure change detector” (V0)
        std::size_t m_lastEntityCount = 0;

        // Timing
        float m_statsAccum = 0.0f;
        float m_refreshAccum = 0.0f;
        float m_inspectorAccum = 0.0f;
        const float m_statsPeriod = 0.10f;
        const float m_refreshPeriod = 0.10f;
        const float m_inspectorPeriod = 1.0f / 30.0f;

        std::function<void()> m_onApply;

        // Tree bookkeeping
        std::unordered_map<EntityID, Rml::Element*> m_rowByEntity;
        std::vector<EntityID>                      m_treeOrder;
        std::unordered_map<EntityID, std::string>  m_cachedLabels;

        // Inspector last pushed values (avoid spamming SetValue)
        std::string m_lastInsName;
        std::string m_lastInsPosX;
        std::string m_lastInsPosY;
        std::string m_lastInsRot;
        std::string m_lastInsScaleX;
        std::string m_lastInsScaleY;
    };

} // namespace KibakoEngine

#else
namespace KibakoEngine { class EditorOverlay {}; }
#endif