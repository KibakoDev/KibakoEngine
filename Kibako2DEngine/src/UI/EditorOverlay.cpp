// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <SDL2/SDL_events.h>

#include <filesystem>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <string>
#include <system_error>
#include <vector>
#include <functional>
#include <unordered_set>
#include <sstream>
#include <iomanip>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Input.h>

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
            candidates.reserve(48);

            std::unordered_set<std::string> visitedRoots;

            auto appendFromRootAndParents = [&](const std::filesystem::path& root) {
                if (root.empty())
                    return;

                std::filesystem::path cursor = root;
                for (int i = 0; i < 8; ++i) {
                    if (cursor.empty())
                        break;

                    const auto inserted = visitedRoots.insert(cursor.generic_string()).second;
                    if (inserted)
                        AppendCandidatesFromRoot(cursor, candidates);

                    if (!cursor.has_parent_path())
                        break;

                    const auto parent = cursor.parent_path();
                    if (parent == cursor)
                        break;

                    cursor = parent;
                }
                };

            appendFromRootAndParents(app.ContentRoot());
            appendFromRootAndParents(app.ExecutableDir());

            std::error_code ec;
            const auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                appendFromRootAndParents(cwd);

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
            errno = 0;
            const float value = std::strtof(begin, &end);
            if (end == begin)
                return false;

            if (errno == ERANGE)
                return false;

            // ignore trailing spaces/tabs
            while (*end == ' ' || *end == '\t')
                ++end;
            if (*end != '\0')
                return false;

            if (!std::isfinite(value))
                return false;

            out = value;
            return true;
        }

        static Rml::String ToRmlString(const std::string& value)
        {
            return Rml::String(value.c_str());
        }

        static std::string FormatFloat(float value)
        {
            std::ostringstream os;
            os.setf(std::ios::fixed, std::ios::floatfield);
            os << std::setprecision(3) << value;
            return os.str();
        }
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;
        m_statsAccum = 0.0f;
        m_refreshAccum = 0.0f;
        m_selectedEntity = 0;
        m_isApplyingInspector = false;
        m_inspectorDirty = false;
        m_hierarchyDirty = true;
        m_lastHierarchySignature = 0;

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
        BindInspectorInputs();
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
        m_isApplyingInspector = false;
        m_inspectorDirty = false;
        m_hierarchyDirty = true;
        m_lastHierarchySignature = 0;
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
        m_inspectorDirty = false;
        m_hierarchyDirty = true;
        m_lastHierarchySignature = 0;
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

    void EditorOverlay::BindInspectorInputs()
    {
        auto bind = [this](Rml::ElementFormControlInput* input) {
            if (!input)
                return;

            input->AddEventListener("change", new ButtonListener([this](Rml::Event&) {
                m_inspectorDirty = true;
                ApplyInspectorLive();
            }));

            input->AddEventListener("blur", new ButtonListener([this](Rml::Event&) {
                m_inspectorDirty = true;
                ApplyInspectorLive();
            }));

            input->AddEventListener("keydown", new ButtonListener([this](Rml::Event& e) {
                if (e.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KI_UNKNOWN)
                    == Rml::Input::KI_RETURN) {
                    m_inspectorDirty = true;
                    ApplyInspectorLive();
                }
            }));
        };

        bind(m_insName);
        bind(m_insPosX);
        bind(m_insPosY);
        bind(m_insRot);
        bind(m_insScaleX);
        bind(m_insScaleY);
    }

    void EditorOverlay::SelectEntity(EntityID id)
    {
        if (m_selectedEntity == id)
            return;

        m_selectedEntity = id;
        m_inspectorDirty = false;
        m_hierarchyDirty = true;
        RefreshInspector();
    }

    void EditorOverlay::Update(float dt)
    {
        if (!m_enabled || !m_doc || !m_app)
            return;

        if (m_inspectorDirty)
            ApplyInspectorLive();

        m_statsAccum += dt;
        m_refreshAccum += dt;
        if (m_statsAccum >= m_statsPeriod) {
            m_statsAccum = 0.0f;
            RefreshStats();
        }

        if (m_refreshAccum >= m_refreshPeriod) {
            m_refreshAccum = 0.0f;

            if (ComputeHierarchySignature() != m_lastHierarchySignature)
                m_hierarchyDirty = true;

            if (m_hierarchyDirty)
                RefreshHierarchy();

            if (!HasFocusedInspectorField())
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
        if (!m_hierarchyList || !m_doc)
            return;

        m_hierarchyList->SetInnerRML("");

        if (!m_scene) {
            auto hint = m_doc->CreateElement("div");
            if (hint) {
                hint->SetClass("hint", true);
                hint->SetInnerRML("No scene loaded.");
                m_hierarchyList->AppendChild(std::move(hint));
            }
            return;
        }

        bool selectionStillValid = false;
        const auto& entities = m_scene->Entities();

        for (const auto& entity : entities) {
            auto button = m_doc->CreateElement("button");
            if (!button)
                continue;

            button->SetClass("entity", true);
            if (!entity.active)
                button->SetClass("inactive", true);

            const auto* name = m_scene->TryGetName(entity.id);
            std::string label = name ? name->name : "";
            if (label.empty()) {
                label = "Entity ";
                label.append(std::to_string(entity.id));
            }

            if (!entity.active)
                label.append(" (inactive)");

            button->SetInnerRML(ToRmlString(label));

            const EntityID id = entity.id;
            button->AddEventListener("click",
                new ButtonListener([this, id](Rml::Event&) {
                    SelectEntity(id);
                    })
            );

            if (id == m_selectedEntity) {
                button->SetClass("selected", true);
                selectionStillValid = true;
            }

            m_hierarchyList->AppendChild(std::move(button));
        }

        if (!selectionStillValid && m_selectedEntity != 0) {
            m_selectedEntity = 0;
            RefreshInspector();
        }

        m_hierarchyDirty = false;
        m_lastHierarchySignature = ComputeHierarchySignature();
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
        if (m_insName) m_insName->SetValue("");
        if (m_insPosX) m_insPosX->SetValue("0.000");
        if (m_insPosY) m_insPosY->SetValue("0.000");
        if (m_insRot) m_insRot->SetValue("0.000");
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
        if (!m_inspectorHint)
            return;

        if (!m_scene) {
            m_inspectorHint->SetClass("hidden", false);
            m_inspectorHint->SetInnerRML("No scene loaded.");
            SetInspectorDefaultValues();
            return;
        }

        const bool hasSelection = m_selectedEntity != 0;
        m_inspectorHint->SetInnerRML("Select an entity to inspect it.");
        m_inspectorHint->SetClass("hidden", hasSelection);

        if (!hasSelection) {
            SetInspectorDefaultValues();
            return;
        }

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity) {
            m_selectedEntity = 0;
            m_inspectorHint->SetClass("hidden", false);
            SetInspectorDefaultValues();
            return;
        }

        const auto* name = m_scene->TryGetName(entity->id);

        const std::string nameText = name ? name->name : "";
        const std::string posXText = FormatFloat(entity->transform.position.x);
        const std::string posYText = FormatFloat(entity->transform.position.y);
        const std::string rotText = FormatFloat(entity->transform.rotation);
        const std::string scaleXText = FormatFloat(entity->transform.scale.x);
        const std::string scaleYText = FormatFloat(entity->transform.scale.y);

        auto maybeSet = [this](Rml::ElementFormControlInput* input, const std::string& value, std::string& lastValue) {
            if (!input)
                return;

            if (!m_isApplyingInspector && input->IsPseudoClassSet("focus"))
                return;

            if (lastValue == value)
                return;

            input->SetValue(ToRmlString(value));
            lastValue = value;
        };

        maybeSet(m_insName, nameText, m_lastInsName);
        maybeSet(m_insPosX, posXText, m_lastInsPosX);
        maybeSet(m_insPosY, posYText, m_lastInsPosY);
        maybeSet(m_insRot, rotText, m_lastInsRot);
        maybeSet(m_insScaleX, scaleXText, m_lastInsScaleX);
        maybeSet(m_insScaleY, scaleYText, m_lastInsScaleY);
    }


    std::uint64_t EditorOverlay::ComputeHierarchySignature() const
    {
        if (!m_scene)
            return 0;

        std::uint64_t signature = 1469598103934665603ull; // FNV-1a
        auto hash = [&signature](std::uint64_t value) {
            signature ^= value;
            signature *= 1099511628211ull;
        };

        const auto& entities = m_scene->Entities();
        hash(static_cast<std::uint64_t>(entities.size()));

        for (const auto& entity : entities) {
            hash(static_cast<std::uint64_t>(entity.id));
            hash(entity.active ? 1ull : 0ull);

            const auto* name = m_scene->TryGetName(entity.id);
            if (!name) {
                hash(0ull);
                continue;
            }

            for (unsigned char c : name->name)
                hash(static_cast<std::uint64_t>(c));
            hash(0xffull);
        }

        return signature;
    }

    void EditorOverlay::ApplyInspectorLive()
    {
        if (!m_inspectorDirty)
            return;

        ApplyInspector();
    }

    void EditorOverlay::ApplyInspector()
    {
        if (!m_scene || m_selectedEntity == 0)
            return;

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity)
            return;

        m_isApplyingInspector = true;

        if (m_insName) {
            const auto& nameValue = m_insName->GetValue();
            if (auto* name = m_scene->TryGetName(entity->id)) {
                if (name->name != nameValue.c_str()) {
                    name->name = nameValue.c_str();
                    m_hierarchyDirty = true;
                }
            }
            else if (!nameValue.empty()) {
                m_scene->AddName(entity->id, nameValue.c_str());
                m_hierarchyDirty = true;
            }
            m_lastInsName = nameValue.c_str();
        }

        float value = 0.0f;
        if (m_insPosX && ParseFloat(m_insPosX->GetValue(), value)) {
            entity->transform.position.x = value;
            m_lastInsPosX = m_insPosX->GetValue().c_str();
        }
        if (m_insPosY && ParseFloat(m_insPosY->GetValue(), value)) {
            entity->transform.position.y = value;
            m_lastInsPosY = m_insPosY->GetValue().c_str();
        }
        if (m_insRot && ParseFloat(m_insRot->GetValue(), value)) {
            entity->transform.rotation = value;
            m_lastInsRot = m_insRot->GetValue().c_str();
        }
        if (m_insScaleX && ParseFloat(m_insScaleX->GetValue(), value)) {
            entity->transform.scale.x = value;
            m_lastInsScaleX = m_insScaleX->GetValue().c_str();
        }
        if (m_insScaleY && ParseFloat(m_insScaleY->GetValue(), value)) {
            entity->transform.scale.y = value;
            m_lastInsScaleY = m_insScaleY->GetValue().c_str();
        }

        m_inspectorDirty = false;
        if (m_hierarchyDirty)
            RefreshHierarchy();
        RefreshInspector();

        m_isApplyingInspector = false;
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD
