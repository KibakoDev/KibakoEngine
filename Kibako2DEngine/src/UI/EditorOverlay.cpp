// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <SDL2/SDL_events.h>

#include <filesystem>
#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>
#include <functional>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
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

        static Rml::Element* GetElement(Rml::ElementDocument& doc, const char* id)
        {
            if (!id || id[0] == '\0')
                return nullptr;

            return doc.GetElementById(id);
        }

        static Rml::ElementFormControlInput* GetInput(Rml::ElementDocument& doc, const char* id)
        {
            if (auto* el = GetElement(doc, id)) {
                return dynamic_cast<Rml::ElementFormControlInput*>(el);
            }
            return nullptr;
        }

        static bool ParseFloat(const Rml::String& text, float& out)
        {
            if (text.empty())
                return false;

            const char* begin = text.c_str();
            char* end = nullptr;
            const float value = std::strtof(begin, &end);
            if (end == begin || *end != '\0')
                return false;

            out = value;
            return true;
        }

        static Rml::String ToRmlString(const std::string& value)
        {
            return Rml::String(value.c_str());
        }

        static Rml::String ToRmlString(float value)
        {
            return Rml::String(std::to_string(value).c_str());
        }
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;
        m_statsAccum = 0.0f;
        m_refreshAccum = 0.0f;

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
        m_hierarchyList = m_doc->GetElementById("hierarchy_list");
        m_inspectorHint = m_doc->GetElementById("inspector_hint");
        m_insName = GetInput(*m_doc, "ins_name");
        m_insPosX = GetInput(*m_doc, "ins_pos_x");
        m_insPosY = GetInput(*m_doc, "ins_pos_y");
        m_insRot = GetInput(*m_doc, "ins_rot");
        m_insScaleX = GetInput(*m_doc, "ins_scale_x");
        m_insScaleY = GetInput(*m_doc, "ins_scale_y");

        if (!m_statsEntities) KbkWarn(kLogChannel, "Missing #stats_entities element");
        if (!m_statsFps)      KbkWarn(kLogChannel, "Missing #stats_fps element");
        if (!m_hierarchyList) KbkWarn(kLogChannel, "Missing #hierarchy_list element");
        if (!m_inspectorHint) KbkWarn(kLogChannel, "Missing #inspector_hint element");
        if (!m_insName)       KbkWarn(kLogChannel, "Missing #ins_name element");
        if (!m_insPosX)       KbkWarn(kLogChannel, "Missing #ins_pos_x element");
        if (!m_insPosY)       KbkWarn(kLogChannel, "Missing #ins_pos_y element");
        if (!m_insRot)        KbkWarn(kLogChannel, "Missing #ins_rot element");
        if (!m_insScaleX)     KbkWarn(kLogChannel, "Missing #ins_scale_x element");
        if (!m_insScaleY)     KbkWarn(kLogChannel, "Missing #ins_scale_y element");

        BindButtons();
        RefreshHierarchy();
        RefreshInspector();

        m_doc->Show();
        if (auto* context = ui.GetContext())
            context->Update();

        KbkLog(kLogChannel, "EditorOverlay initialized (loaded '%s')", loadedPath.c_str());
    }

    void EditorOverlay::Shutdown()
    {
        m_statsEntities = nullptr;
        m_statsFps = nullptr;
        m_hierarchyList = nullptr;
        m_inspectorHint = nullptr;
        m_insName = nullptr;
        m_insPosX = nullptr;
        m_insPosY = nullptr;
        m_insRot = nullptr;
        m_insScaleX = nullptr;
        m_insScaleY = nullptr;
        m_selectedEntity = 0;

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
        m_refreshAccum = 0.0f;
    }

    void EditorOverlay::SetScene(Scene2D* scene)
    {
        m_scene = scene;
        m_selectedEntity = 0;
        RefreshHierarchy();
        RefreshInspector();
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
                    ApplyInspector();
                    if (m_onApply) m_onApply();
                    })
            );
        }
        else {
            KbkWarn(kLogChannel, "Missing #btn_apply element");
        }
    }

    void EditorOverlay::SelectEntity(EntityID id)
    {
        if (m_selectedEntity == id)
            return;

        m_selectedEntity = id;
        RefreshInspector();
    }

    void EditorOverlay::Update(float dt)
    {
        if (!m_enabled || !m_doc || !m_app)
            return;

        m_statsAccum += dt;
        m_refreshAccum += dt;
        if (m_statsAccum >= m_statsPeriod) {
            m_statsAccum = 0.0f;
            RefreshStats();
        }

        if (m_refreshAccum >= m_refreshPeriod) {
            m_refreshAccum = 0.0f;
            RefreshHierarchy();
            RefreshInspector();
        }
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

    void EditorOverlay::RefreshHierarchy()
    {
        if (!m_hierarchyList)
            return;

        m_hierarchyList->SetInnerRML("");

        if (!m_scene) {
            auto* hint = m_doc ? m_doc->CreateElement("div") : nullptr;
            if (hint) {
                hint->SetClass("hint", true);
                hint->SetInnerRML("No scene loaded.");
                m_hierarchyList->AppendChild(hint);
            }
            return;
        }

        bool selectionStillValid = false;

        for (const auto& entity : m_scene->Entities()) {
            if (!entity.active)
                continue;

            auto* button = m_doc->CreateElement("button");
            if (!button)
                continue;

            button->SetClass("entity", true);

            const auto* name = m_scene->TryGetName(entity.id);
            std::string label = name ? name->name : "";
            if (label.empty()) {
                label = "Entity ";
                label.append(std::to_string(entity.id));
            }

            button->SetInnerRML(ToRmlString(label));

            const EntityID id = entity.id;
            button->AddEventListener("click",
                new ButtonListener([this, id](Rml::Event&) {
                    SelectEntity(id);
                    RefreshHierarchy();
                    })
            );

            if (id == m_selectedEntity) {
                button->SetClass("selected", true);
                selectionStillValid = true;
            }

            m_hierarchyList->AppendChild(button);
        }

        if (!selectionStillValid && m_selectedEntity != 0) {
            m_selectedEntity = 0;
            RefreshInspector();
        }
    }

    void EditorOverlay::RefreshInspector()
    {
        if (!m_inspectorHint)
            return;

        if (!m_scene) {
            m_inspectorHint->SetClass("hidden", false);
            m_inspectorHint->SetInnerRML("No scene loaded.");
            if (m_insName) m_insName->SetValue("");
            if (m_insPosX) m_insPosX->SetValue("0");
            if (m_insPosY) m_insPosY->SetValue("0");
            if (m_insRot) m_insRot->SetValue("0");
            if (m_insScaleX) m_insScaleX->SetValue("1");
            if (m_insScaleY) m_insScaleY->SetValue("1");
            return;
        }

        const bool hasSelection = m_selectedEntity != 0;
        m_inspectorHint->SetInnerRML("Select an entity to inspect it.");
        m_inspectorHint->SetClass("hidden", hasSelection);

        if (!hasSelection) {
            if (m_insName) m_insName->SetValue("");
            if (m_insPosX) m_insPosX->SetValue("0");
            if (m_insPosY) m_insPosY->SetValue("0");
            if (m_insRot) m_insRot->SetValue("0");
            if (m_insScaleX) m_insScaleX->SetValue("1");
            if (m_insScaleY) m_insScaleY->SetValue("1");
            return;
        }

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity || !entity->active) {
            m_selectedEntity = 0;
            m_inspectorHint->SetClass("hidden", false);
            return;
        }

        const auto* name = m_scene->TryGetName(entity->id);
        if (m_insName)
            m_insName->SetValue(ToRmlString(name ? name->name : ""));
        if (m_insPosX)
            m_insPosX->SetValue(ToRmlString(entity->transform.position.x));
        if (m_insPosY)
            m_insPosY->SetValue(ToRmlString(entity->transform.position.y));
        if (m_insRot)
            m_insRot->SetValue(ToRmlString(entity->transform.rotation));
        if (m_insScaleX)
            m_insScaleX->SetValue(ToRmlString(entity->transform.scale.x));
        if (m_insScaleY)
            m_insScaleY->SetValue(ToRmlString(entity->transform.scale.y));
    }

    void EditorOverlay::ApplyInspector()
    {
        if (!m_scene || m_selectedEntity == 0)
            return;

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity || !entity->active)
            return;

        if (m_insName) {
            const auto& nameValue = m_insName->GetValue();
            if (auto* name = m_scene->TryGetName(entity->id)) {
                name->name = nameValue.c_str();
            }
            else if (!nameValue.empty()) {
                m_scene->AddName(entity->id, nameValue.c_str());
            }
        }

        float value = 0.0f;
        if (m_insPosX && ParseFloat(m_insPosX->GetValue(), value))
            entity->transform.position.x = value;
        if (m_insPosY && ParseFloat(m_insPosY->GetValue(), value))
            entity->transform.position.y = value;
        if (m_insRot && ParseFloat(m_insRot->GetValue(), value))
            entity->transform.rotation = value;
        if (m_insScaleX && ParseFloat(m_insScaleX->GetValue(), value))
            entity->transform.scale.x = value;
        if (m_insScaleY && ParseFloat(m_insScaleY->GetValue(), value))
            entity->transform.scale.y = value;

        RefreshHierarchy();
        RefreshInspector();
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD
