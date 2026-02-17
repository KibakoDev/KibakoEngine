// KibakoEngine/UI/EditorOverlay.h
#pragma once

#include "KibakoEngine/Core/Debug.h"

#if KBK_DEBUG_BUILD

#include <cstdint>
#include <functional>
#include <string>

namespace Rml {
    class ElementDocument;
    class Element;
    class ElementFormControlInput;
}

namespace KibakoEngine {

    class Application;
    class Scene2D;

    using EntityID = std::uint32_t;

    class EditorOverlay
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
        void BindButtons();
        void BindInspectorInputs();

        void RefreshStats();
        void RefreshHierarchy();
        void RefreshInspector();

        void SelectEntity(EntityID id);

        void ApplyInspector();
        void ApplyInspectorLive();

        bool HasFocusedInspectorField() const;

        void SetInspectorDefaultValues();

    private:
        Application* m_app = nullptr;
        Scene2D* m_scene = nullptr;

        // Document + elements
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

        // State
        std::function<void()> m_onApply;

        float   m_statsAccum = 0.0f;
        float   m_refreshAccum = 0.0f;
        float   m_statsPeriod = 0.10f;   // 10x/sec
        float   m_refreshPeriod = 0.15f; // UI refresh cadence

        bool    m_enabled = true;

        EntityID m_selectedEntity = 0;

        bool    m_inspectorDirty = false;
        bool    m_isApplyingInspector = false;
        bool    m_hierarchyDirty = true;

        // Last values written to inputs (avoid spam)
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

#endif // KBK_DEBUG_BUILD