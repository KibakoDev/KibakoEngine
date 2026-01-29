#pragma once

#include <cstdint>

#if KBK_DEBUG_BUILD

namespace KibakoEngine {

    class Application;
    class Scene2D;

    namespace Rml {
        class ElementDocument;
        class Element;
    }

    // ------------------------------------------------------------
    // Engine Debug Editor Overlay (Debug builds only)
    // ------------------------------------------------------------
    class EditorOverlay
    {
    public:
        void Init(Application& app);
        void Shutdown();

        void SetScene(Scene2D* scene) { m_scene = scene; }

        void SetEnabled(bool enabled);
        bool IsEnabled() const { return m_enabled; }

        void Update(float dt);

    private:
        void RefreshStats();

    private:
        Application* m_app = nullptr;
        Scene2D* m_scene = nullptr;

        Rml::ElementDocument* m_doc = nullptr;
        Rml::Element* m_statsEntities = nullptr;
        Rml::Element* m_statsFps = nullptr;

        float m_statsAccum = 0.0f;   // throttling accumulator
        float m_statsPeriod = 0.10f;  // update 10x/sec
        bool  m_enabled = true;
    };

} // namespace KibakoEngine

#else
// ------------------------------------------------------------
// Release build stub (compiled out)
// ------------------------------------------------------------
namespace KibakoEngine {

    class Application;
    class Scene2D;

    class EditorOverlay
    {
    public:
        void Init(Application&) {}
        void Shutdown() {}
        void SetScene(Scene2D*) {}
        void SetEnabled(bool) {}
        bool IsEnabled() const { return false; }
        void Update(float) {}
    };

} // namespace KibakoEngine
#endif // KBK_DEBUG_BUILD