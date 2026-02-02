// KibakoEngine/UI/EditorOverlay.h
#pragma once

#include "KibakoEngine/Core/Debug.h"

#if KBK_DEBUG_BUILD

#include <functional>

namespace Rml {
    class ElementDocument;
    class Element;
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

        void SetScene(Scene2D* scene) { m_scene = scene; }

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }

        // Optional hooks for UI buttons
        void SetOnApply(std::function<void()> fn) { m_onApply = std::move(fn); }

        void Update(float dt);

    private:
        void BindButtons();
        void RefreshStats();

    private:
        Application* m_app = nullptr;
        Scene2D* m_scene = nullptr;

        Rml::ElementDocument* m_doc = nullptr;
        Rml::Element* m_statsEntities = nullptr;
        Rml::Element* m_statsFps = nullptr;

        std::function<void()> m_onApply;

        float m_statsAccum = 0.0f;
        float m_statsPeriod = 0.10f; // 10x/sec
        bool  m_enabled = true;
    };

} // namespace KibakoEngine

#else

// Release build: compiled out
namespace KibakoEngine { class EditorOverlay {}; }

#endif // KBK_DEBUG_BUILD