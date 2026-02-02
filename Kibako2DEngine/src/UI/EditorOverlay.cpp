// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";

        // RmlUI is generally happier with forward slashes, even on Windows.
        static std::string ToRmlPathString(const std::filesystem::path& p)
        {
            // generic_string() => forward slashes
            return p.generic_string();
        }

        static bool ExistsNoThrow(const std::filesystem::path& p)
        {
            std::error_code ec;
            const bool ok = std::filesystem::exists(p, ec);
            return ok && !ec;
        }

        static bool TryLoad(RmlUIContext& ui, const std::filesystem::path& p, Rml::ElementDocument*& outDoc)
        {
            outDoc = nullptr;
            const std::string s = ToRmlPathString(p);
            if (auto* doc = ui.LoadDocument(s.c_str())) {
                outDoc = doc;
                return true;
            }
            return false;
        }

        static void AppendCandidatesFromRoot(const std::filesystem::path& root,
            std::vector<std::filesystem::path>& out)
        {
            if (root.empty())
                return;

            out.emplace_back(root / "assets" / "ui" / "editor.rml");
            out.emplace_back(root / "Kibako2DEngine" / "assets" / "ui" / "editor.rml");
        }

        static std::vector<std::filesystem::path> BuildCandidates(const Application& app)
        {
            std::vector<std::filesystem::path> candidates;
            candidates.reserve(8);

            AppendCandidatesFromRoot(app.ContentRoot(), candidates);
            AppendCandidatesFromRoot(app.ExecutableDir(), candidates);

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                AppendCandidatesFromRoot(cwd, candidates);

            return candidates;
        }
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;
        m_statsAccum = 0.0f;

        auto& ui = app.UI();

        // Already initialized? Avoid double-load.
        if (m_doc) {
            KbkWarn(kLogChannel, "Init called but overlay already has a document loaded.");
            return;
        }

        const auto candidates = BuildCandidates(app);
        Rml::ElementDocument* loaded = nullptr;
        std::string loadedPath;

        for (const auto& p : candidates) {
            // Only attempt if it exists on disk (saves noisy failures)
            if (!ExistsNoThrow(p))
                continue;

            if (TryLoad(ui, p, loaded)) {
                loadedPath = ToRmlPathString(p);
                break;
            }
        }

        if (!loaded) {
            std::string list;
            list.reserve(512);
            for (const auto& p : candidates) {
                list.append(" - ");
                list.append(ToRmlPathString(p));
                list.push_back('\n');
            }
            KbkError(kLogChannel,
                "Failed to load editor UI. Looked for:\n%s",
                list.c_str());
            return;
        }

        m_doc = loaded;

        m_statsEntities = m_doc->GetElementById("stats_entities");
        m_statsFps = m_doc->GetElementById("stats_fps");

        if (!m_statsEntities) KbkWarn(kLogChannel, "Missing #stats_entities element");
        if (!m_statsFps)      KbkWarn(kLogChannel, "Missing #stats_fps element");

        // Show doc
        m_doc->Show();
        if (auto* context = ui.GetContext())
            context->Update();

        KbkLog(kLogChannel, "EditorOverlay initialized (loaded '%s')", loadedPath.c_str());
    }

    void EditorOverlay::Shutdown()
    {
        m_statsEntities = nullptr;
        m_statsFps = nullptr;

        if (m_doc) {
            m_doc->Hide();
            m_doc->Close(); // releases the document from the Rml context
            m_doc = nullptr;
        }

        m_scene = nullptr;
        m_app = nullptr;
        m_enabled = false;
        m_statsAccum = 0.0f;
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
