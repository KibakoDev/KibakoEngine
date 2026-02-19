// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <SDL2/SDL_events.h>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";

        static std::string ToRmlPathString(const std::filesystem::path& p)
        {
            return p.generic_string(); // forward slashes
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

        static std::vector<std::filesystem::path> BuildCandidates(const Application& app)
        {
            std::vector<std::filesystem::path> candidates;
            candidates.reserve(8);

            // Stable engine-owned path (your rule)
            if (!app.EngineRoot().empty())
                candidates.emplace_back(app.EngineRoot() / "assets" / "ui" / "editor.rml");

            // Dev fallbacks (optional)
            if (!app.ContentRoot().empty())
                candidates.emplace_back(app.ContentRoot() / "assets" / "ui" / "editor.rml");

            if (!app.ExecutableDir().empty())
                candidates.emplace_back(app.ExecutableDir() / "assets" / "ui" / "editor.rml");

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                candidates.emplace_back(cwd / "assets" / "ui" / "editor.rml");

            return candidates;
        }

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
            if (!id || id[0] == '\0') return nullptr;
            return doc.GetElementById(id);
        }

        static Rml::ElementFormControlInput* GetInput(Rml::ElementDocument& doc, const char* id)
        {
            if (auto* el = GetElement(doc, id))
                return dynamic_cast<Rml::ElementFormControlInput*>(el);
            return nullptr;
        }

        static bool ParseFloat(const Rml::String& text, float& out)
        {
            if (text.empty())
                return false;

            const char* begin = text.c_str();
            char* end = nullptr;

            errno = 0;
            const float v = std::strtof(begin, &end);

            if (end == begin) return false;
            if (errno == ERANGE) return false;

            while (*end == ' ' || *end == '\t') ++end;
            if (*end != '\0') return false;

            if (!std::isfinite(v)) return false;

            out = v;
            return true;
        }

        static std::string FormatFloat(float value)
        {
            std::ostringstream os;
            os.setf(std::ios::fixed, std::ios::floatfield);
            os << std::setprecision(3) << value;
            return os.str();
        }
    } // namespace

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;

        m_statsAccum = 0.0f;
        m_refreshAccum = 0.0f;

        m_selectedEntity = 0;
        m_isApplyingInspector = false;
        m_hierarchyDirty = true;
        m_inspectorViewDirty = true;
        m_statsDirty = true;
        m_lastSceneRevision = 0;
        m_entityButtons.clear();
        m_hierarchyOrder.clear();

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

        // Cache UI elements
        m_statsEntities = GetElement(*m_doc, "stats_entities");
        m_statsFps = GetElement(*m_doc, "stats_fps");
        m_hierarchyList = GetElement(*m_doc, "hierarchy_list");

        m_inspectorHint = GetElement(*m_doc, "inspector_hint");
        m_insName = GetInput(*m_doc, "ins_name");
        m_insPosX = GetInput(*m_doc, "ins_pos_x");
        m_insPosY = GetInput(*m_doc, "ins_pos_y");
        m_insRot = GetInput(*m_doc, "ins_rot");
        m_insScaleX = GetInput(*m_doc, "ins_scale_x");
        m_insScaleY = GetInput(*m_doc, "ins_scale_y");

        // Warn loudly if ids mismatch (this is THE #1 reason inspector doesn't update)
        if (!m_hierarchyList) KbkWarn(kLogChannel, "Missing #hierarchy_list element");
        if (!m_inspectorHint) KbkWarn(kLogChannel, "Missing #inspector_hint element");
        if (!m_insName)       KbkWarn(kLogChannel, "Missing #ins_name element");
        if (!m_insPosX)       KbkWarn(kLogChannel, "Missing #ins_pos_x element");
        if (!m_insPosY)       KbkWarn(kLogChannel, "Missing #ins_pos_y element");
        if (!m_insRot)        KbkWarn(kLogChannel, "Missing #ins_rot element");
        if (!m_insScaleX)     KbkWarn(kLogChannel, "Missing #ins_scale_x element");
        if (!m_insScaleY)     KbkWarn(kLogChannel, "Missing #ins_scale_y element");

        BindButtons();

        RefreshStats();
        RefreshHierarchy();
        m_inspectorViewDirty = true;
        RefreshInspector();

        m_doc->Show();
        if (auto* ctx = ui.GetContext())
            ctx->Update();

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
        m_isApplyingInspector = false;
        m_hierarchyDirty = true;
        m_inspectorViewDirty = true;
        m_statsDirty = true;
        m_lastSceneRevision = 0;
        m_entityButtons.clear();
        m_hierarchyOrder.clear();

        m_lastInsName.clear();
        m_lastInsPosX.clear();
        m_lastInsPosY.clear();
        m_lastInsRot.clear();
        m_lastInsScaleX.clear();
        m_lastInsScaleY.clear();

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
        m_hierarchyDirty = true;
        m_inspectorViewDirty = true;
        m_statsDirty = true;
        m_lastSceneRevision = 0;

        RefreshStats();
        RefreshHierarchy();
        m_inspectorViewDirty = true;
        RefreshInspector();
    }

    void EditorOverlay::SetEnabled(bool enabled)
    {
        m_enabled = enabled;
        if (!m_doc) return;

        if (m_enabled) m_doc->Show();
        else           m_doc->Hide();
    }

    void EditorOverlay::BindButtons()
    {
        if (!m_doc) return;

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

        m_inspectorViewDirty = true;
        m_hierarchyDirty = true;
    }

    void EditorOverlay::Update(float dt)
    {
        if (!m_enabled || !m_doc || !m_app)
            return;

        KBK_PROFILE_SCOPE("EditorOverlay::Update");

        if (m_scene) {
            const auto revision = m_scene->Revision();
            if (revision != m_lastSceneRevision) {
                m_lastSceneRevision = revision;
                m_hierarchyDirty = true;
                m_inspectorViewDirty = true;
                m_statsDirty = true;
            }
        }

        m_statsAccum += dt;
        m_refreshAccum += dt;

        if (m_statsAccum >= m_statsPeriod) {
            m_statsAccum = 0.0f;
            m_statsDirty = true;
        }

        if (m_statsDirty) {
            RefreshStats();
            m_statsDirty = false;
        }

        if (m_refreshAccum >= m_refreshPeriod) {
            m_refreshAccum = 0.0f;

            if (m_hierarchyDirty)
                RefreshHierarchy();

            // Don't override user typing
            if (m_inspectorViewDirty && !HasFocusedInspectorField())
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
        KBK_PROFILE_SCOPE("EditorOverlay::RefreshHierarchy");

        if (!m_hierarchyList || !m_doc)
            return;

        if (m_entityButtons.empty())
            RebuildHierarchy();
        else
            SyncHierarchyIncremental();

        m_hierarchyDirty = false;
    }

    void EditorOverlay::RebuildHierarchy()
    {
        KBK_PROFILE_SCOPE("EditorOverlay::RebuildHierarchy");

        m_hierarchyList->SetInnerRML("");
        m_entityButtons.clear();
        m_hierarchyOrder.clear();

        if (!m_scene) {
            auto hint = m_doc->CreateElement("div");
            if (hint) {
                hint->SetClass("hint", true);
                hint->SetInnerRML("No scene loaded.");
                m_hierarchyList->AppendChild(std::move(hint));
            }
            return;
        }

        for (const auto& entity : m_scene->Entities()) {
            auto button = m_doc->CreateElement("button");
            if (!button)
                continue;

            button->SetClass("entity", true);
            if (!entity.active)
                button->SetClass("inactive", true);

            const auto* name = m_scene->TryGetName(entity.id);
            std::string label = (name && !name->name.empty()) ? name->name : ("Entity " + std::to_string(entity.id));
            if (!entity.active)
                label.append(" (inactive)");
            button->SetInnerRML(label.c_str());

            const EntityID id = entity.id;
            button->AddEventListener("click", new ButtonListener([this, id](Rml::Event&) { SelectEntity(id); }));

            if (id == m_selectedEntity)
                button->SetClass("selected", true);

            Rml::Element* raw = button.get();
            m_hierarchyList->AppendChild(std::move(button));
            m_entityButtons[id] = raw;
            m_hierarchyOrder.push_back(id);
        }
    }

    void EditorOverlay::SyncHierarchyIncremental()
    {
        KBK_PROFILE_SCOPE("EditorOverlay::SyncHierarchyIncremental");

        if (!m_scene) {
            RebuildHierarchy();
            m_selectedEntity = 0;
            m_inspectorViewDirty = true;
            return;
        }

        std::vector<EntityID> nextOrder;
        nextOrder.reserve(m_scene->Entities().size());

        bool selectionStillValid = false;

        for (const auto& entity : m_scene->Entities()) {
            nextOrder.push_back(entity.id);

            auto found = m_entityButtons.find(entity.id);
            if (found == m_entityButtons.end() || !found->second) {
                RebuildHierarchy();
                return;
            }

            auto* button = found->second;
            button->SetClass("entity", true);
            button->SetClass("inactive", !entity.active);
            button->SetClass("selected", entity.id == m_selectedEntity);

            const auto* name = m_scene->TryGetName(entity.id);
            std::string label = (name && !name->name.empty()) ? name->name : ("Entity " + std::to_string(entity.id));
            if (!entity.active)
                label.append(" (inactive)");
            button->SetInnerRML(label.c_str());

            if (entity.id == m_selectedEntity)
                selectionStillValid = true;
        }

        if (nextOrder.size() != m_hierarchyOrder.size()) {
            RebuildHierarchy();
            return;
        }

        for (std::size_t i = 0; i < nextOrder.size(); ++i) {
            if (nextOrder[i] != m_hierarchyOrder[i]) {
                RebuildHierarchy();
                return;
            }
        }

        if (!selectionStillValid && m_selectedEntity != 0) {
            m_selectedEntity = 0;
            m_inspectorViewDirty = true;
        }

        m_hierarchyOrder = std::move(nextOrder);
    }

    bool EditorOverlay::HasFocusedInspectorField() const
    {
        auto focused = [](const Rml::ElementFormControlInput* input) {
            return input && input->IsPseudoClassSet("focus");
            };

        return focused(m_insName)
            || focused(m_insPosX)
            || focused(m_insPosY)
            || focused(m_insRot)
            || focused(m_insScaleX)
            || focused(m_insScaleY);
    }

    void EditorOverlay::SetInspectorDefaultValues()
    {
        if (m_insName)   m_insName->SetValue("");
        if (m_insPosX)   m_insPosX->SetValue("0.000");
        if (m_insPosY)   m_insPosY->SetValue("0.000");
        if (m_insRot)    m_insRot->SetValue("0.000");
        if (m_insScaleX) m_insScaleX->SetValue("1.000");
        if (m_insScaleY) m_insScaleY->SetValue("1.000");

        m_lastInsName.clear();
        m_lastInsPosX = "0.000";
        m_lastInsPosY = "0.000";
        m_lastInsRot = "0.000";
        m_lastInsScaleX = "1.000";
        m_lastInsScaleY = "1.000";
    }

    void EditorOverlay::RefreshInspector()
    {
        KBK_PROFILE_SCOPE("EditorOverlay::RefreshInspector");

        if (!m_inspectorHint)
            return;

        if (!m_scene) {
            m_inspectorHint->SetClass("hidden", false);
            m_inspectorHint->SetInnerRML("No scene loaded.");
            SetInspectorDefaultValues();
            m_inspectorViewDirty = false;
            return;
        }

        const bool hasSelection = (m_selectedEntity != 0);

        m_inspectorHint->SetInnerRML("Select an entity to inspect it.");
        m_inspectorHint->SetClass("hidden", hasSelection);

        if (!hasSelection) {
            SetInspectorDefaultValues();
            m_inspectorViewDirty = false;
            return;
        }

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity) {
            m_selectedEntity = 0;
            m_inspectorHint->SetClass("hidden", false);
            SetInspectorDefaultValues();
            m_inspectorViewDirty = false;
            return;
        }

        const auto* name = m_scene->TryGetName(entity->id);

        const std::string nameText = name ? name->name : "";
        const std::string posXText = FormatFloat(entity->transform.position.x);
        const std::string posYText = FormatFloat(entity->transform.position.y);
        const std::string rotText = FormatFloat(entity->transform.rotation);
        const std::string scaleXText = FormatFloat(entity->transform.scale.x);
        const std::string scaleYText = FormatFloat(entity->transform.scale.y);

        auto maybeSet = [this](Rml::ElementFormControlInput* input,
            const std::string& value,
            std::string& lastValue)
            {
                if (!input) return;

                // dont fight user typing (except during Apply)
                if (!m_isApplyingInspector && input->IsPseudoClassSet("focus"))
                    return;

                if (lastValue == value)
                    return;

                input->SetValue(value.c_str());
                lastValue = value;
            };

        maybeSet(m_insName, nameText, m_lastInsName);
        maybeSet(m_insPosX, posXText, m_lastInsPosX);
        maybeSet(m_insPosY, posYText, m_lastInsPosY);
        maybeSet(m_insRot, rotText, m_lastInsRot);
        maybeSet(m_insScaleX, scaleXText, m_lastInsScaleX);
        maybeSet(m_insScaleY, scaleYText, m_lastInsScaleY);

        m_inspectorViewDirty = false;
    }

    void EditorOverlay::ApplyInspector()
    {
        if (!m_scene || m_selectedEntity == 0)
            return;

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity)
            return;

        m_isApplyingInspector = true;

        // Name
        if (m_insName) {
            const auto& nameValue = m_insName->GetValue();
            if (auto* name = m_scene->TryGetName(entity->id)) {
                if (name->name != nameValue.c_str()) {
                    name->name = nameValue.c_str();
                    m_hierarchyDirty = true;
                    m_inspectorViewDirty = true;
                }
            }
            else if (!nameValue.empty()) {
                m_scene->AddName(entity->id, nameValue.c_str());
                m_hierarchyDirty = true;
                m_inspectorViewDirty = true;
            }
            m_lastInsName = nameValue.c_str();
        }

        // Transform
        float v = 0.0f;

        if (m_insPosX && ParseFloat(m_insPosX->GetValue(), v)) {
            entity->transform.position.x = v;
            m_lastInsPosX = m_insPosX->GetValue().c_str();
        }
        if (m_insPosY && ParseFloat(m_insPosY->GetValue(), v)) {
            entity->transform.position.y = v;
            m_lastInsPosY = m_insPosY->GetValue().c_str();
        }
        if (m_insRot && ParseFloat(m_insRot->GetValue(), v)) {
            entity->transform.rotation = v;
            m_lastInsRot = m_insRot->GetValue().c_str();
        }
        if (m_insScaleX && ParseFloat(m_insScaleX->GetValue(), v)) {
            entity->transform.scale.x = v;
            m_lastInsScaleX = m_insScaleX->GetValue().c_str();
        }
        if (m_insScaleY && ParseFloat(m_insScaleY->GetValue(), v)) {
            entity->transform.scale.y = v;
            m_lastInsScaleY = m_insScaleY->GetValue().c_str();
        }


        if (m_hierarchyDirty)
            RefreshHierarchy();

        m_inspectorViewDirty = true;
        RefreshInspector();

        m_isApplyingInspector = false;
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD
