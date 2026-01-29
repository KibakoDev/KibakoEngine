#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;

        // Engine-owned document (assets path we decide next)
        auto& ui = app.UI();
        m_doc = ui.LoadDocument("assets/ui/editor.rml");
        if (!m_doc) {
            KbkError(kLogChannel, "Failed to load assets/ui/editor.rml");
            return;
        }

        m_statsEntities = m_doc->GetElementById("stats_entities");
        m_statsFps = m_doc->GetElementById("stats_fps");

        if (!m_statsEntities) KbkWarn(kLogChannel, "Missing #stats_entities element");
        if (!m_statsFps)      KbkWarn(kLogChannel, "Missing #stats_fps element");

        m_doc->Show();
        ui.GetContext()->Update();

        KbkLog(kLogChannel, "EditorOverlay initialized");
    }

    void EditorOverlay::Shutdown()
    {
        m_statsEntities = nullptr;
        m_statsFps = nullptr;

        if (m_doc) {
            m_doc->Hide();
            m_doc = nullptr;
        }

        m_scene = nullptr;
        m_app = nullptr;
    }

    void EditorOverlay::SetEnabled(bool enabled)
    {
        m_enabled = enabled;

        if (!m_doc)
            return;

        if (m_enabled) m_doc->Show();
        else           m_doc->Hide();
    }

    void EditorOverlay::Update(float dt)
    {
        if (!m_enabled || !m_doc || !m_app)
            return;

        m_statsAccum += dt;
        if (m_statsAccum < m_statsPeriod)
            return;

        m_statsAccum = 0.0f;
        RefreshStats();
    }

    void EditorOverlay::RefreshStats()
    {
        // Entities
        if (m_statsEntities) {
            if (!m_scene) {
                m_statsEntities->SetInnerRML("Entities: (no scene)");
            }
            else {
                const auto& ents = m_scene->Entities();

                int activeCount = 0;
                for (const auto& e : ents)
                    if (e.active) ++activeCount;

                const std::string txt =
                    "Entities: " + std::to_string(ents.size()) +
                    " (active " + std::to_string(activeCount) + ")";

                m_statsEntities->SetInnerRML(txt.c_str());
            }
        }

        // FPS
        if (m_statsFps) {
            const double fps = m_app->TimeSys().FPS();
            const int fpsInt = static_cast<int>(fps + 0.5);
            const std::string txt = "FPS: " + std::to_string(fpsInt);
            m_statsFps->SetInnerRML(txt.c_str());
        }
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD