// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <SDL2/SDL_events.h>

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>
#include <functional>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";

        // RmlUI prefers forward slashes even on Windows.
        static std::string ToRmlPathString(const std::filesystem::path& p)
        {
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

            // game-style
            out.emplace_back(root / "assets" / "ui" / "editor.rml");

            // engine-style root containing Kibako2DEngine
            out.emplace_back(root / "Kibako2DEngine" / "assets" / "ui" / "editor.rml");
        }

        static std::vector<std::filesystem::path> BuildCandidates(const Application& app)
        {
            std::vector<std::filesystem::path> candidates;
            candidates.reserve(12);

            AppendCandidatesFromRoot(app.ContentRoot(), candidates);
            AppendCandidatesFromRoot(app.ExecutableDir(), candidates);

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                AppendCandidatesFromRoot(cwd, candidates);

            return candidates;
        }

        // Small helper listener (self-deletes on detach)
        class ButtonListener final : public Rml::EventListener
        {
        public:
            using Callback = std::function<void(Rml::Event&)>;
            explicit ButtonListener(Callback cb) : m_cb(std::move(cb)) {}

            void ProcessEvent(Rml::Event& e) override { if (m_cb) m_cb(e); }
            void OnDetach(Rml::Element*) override { delete this; }

        private:
            Callback m_cb;
        };
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;
        m_statsAccum = 0.0f;

        auto& ui = app.UI();

        if (m_doc) {
            KbkWarn(kLogChannel, "Init called but overlay already has a document loaded.");
            return;
        }

        const auto candidates = BuildCandidates(app);

        Rml::ElementDocument* loadedDoc = nullptr;
        std::string loadedPath;

        for (const auto& p : candidates) {
            if (!ExistsNoThrow(p))
                continue;

            if (TryLoad(ui, p, loadedDoc)) {
                loadedPath = ToRmlPathString(p);
                break;
            }
        }

        if (!loadedDoc) {
            std::string list;
            list.reserve(512);
            for (const auto& p : candidates) {
                list.append(" - ");
                list.append(ToRmlPathString(p));
                list.push_back('\n');
            }
            KbkError(kLogChannel, "Failed to load editor UI. Looked for:\n%s", list.c_str());
            return;
        }

        m_doc = loadedDoc;

        m_statsEntities = m_doc->GetElementById("stats_entities");
        m_statsFps = m_doc->GetElementById("stats_fps");

        if (!m_statsEntities) KbkWarn(kLogChannel, "Missing #stats_entities element");
        if (!m_statsFps)      KbkWarn(kLogChannel, "Missing #stats_fps element");

        BindButtons();

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
            m_doc->Close();
            m_doc = nullptr;
        }

        m_scene = nullptr;
        m_app = nullptr;
        m_onApply = {};
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

    void EditorOverlay::BindButtons()
    {
        if (!m_doc)
            return;

        if (auto* quit = m_doc->GetElementById("btn_quit")) {
            quit->AddEventListener("click",
                new ButtonListener([](Rml::Event&) {
                    SDL_Event evt{};
                    evt.type = SDL_QUIT;
                    SDL_PushEvent(&evt);
                    })
            );
        }
        else {
            KbkWarn(kLogChannel, "Missing #btn_quit element");
        }

        if (auto* apply = m_doc->GetElementById("btn_apply")) {
            apply->AddEventListener("click",
                new ButtonListener([this](Rml::Event&) {
                    // For now, delegate to user/game hook.
                    if (m_onApply) m_onApply();
                    })
            );
        }
        else {
            KbkWarn(kLogChannel, "Missing #btn_apply element");
        }
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
