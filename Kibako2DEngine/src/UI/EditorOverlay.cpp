#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <array>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <filesystem>
#include <string>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;

        auto& ui = app.UI();
        std::string editorPath = "assets/ui/editor.rml";
        if (!std::filesystem::exists(editorPath)) {
            const std::string legacyPath = "Kibako2DEngine/assets/ui/editor.rml";
            if (std::filesystem::exists(legacyPath))
                editorPath = legacyPath;
        }
        m_doc = ui.LoadDocument(editorPath.c_str());
        if (!m_doc) {
            KbkError(kLogChannel, "Failed to load editor UI from '%s'", editorPath.c_str());
            return;
        }

        m_statsEntities = m_doc->GetElementById("stats_entities");
        m_statsFps = m_doc->GetElementById("stats_fps");

        if (!m_statsEntities) KbkWarn(kLogChannel, "Missing #stats_entities element");
        if (!m_statsFps)      KbkWarn(kLogChannel, "Missing #stats_fps element");

        m_doc->Show();
        if (auto* context = ui.GetContext())
            context->Update();

        KbkLog(kLogChannel, "EditorOverlay initialized");
    }

    void EditorOverlay::Shutdown()
    {
        m_statsEntities = nullptr;
        m_statsFps = nullptr;

        if (m_doc) {
            m_doc->Hide();
            m_doc->Close();
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

                std::string text;
                text.reserve(64);
                text.append("Entities: ");
                text.append(std::to_string(ents.size()));
                text.append(" (active ");
                text.append(std::to_string(activeCount));
                text.append(")");

                m_statsEntities->SetInnerRML(text.c_str());
            }
        }

        // FPS
        if (m_statsFps) {
            const double fps = m_app->TimeSys().FPS();
            const int fpsInt = static_cast<int>(fps + 0.5);
            std::string text;
            text.reserve(16);
            text.append("FPS: ");
            text.append(std::to_string(fpsInt));
            m_statsFps->SetInnerRML(text.c_str());
        }
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD