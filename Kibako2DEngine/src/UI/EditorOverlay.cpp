// KibakoEngine/UI/EditorOverlay.cpp
#include "KibakoEngine/UI/EditorOverlay.h"

#if KBK_DEBUG_BUILD

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Scene/Scene2D.h"
#include "KibakoEngine/UI/RmlUIContext.h"

#include <SDL2/SDL_events.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <cstring>

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "Kibako.EditorUI";

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

            out.emplace_back(root / "assets" / "ui" / "editor.rml");
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

        class MethodEventListener final : public Rml::EventListener
        {
        public:
            using Callback = std::function<void(Rml::Event&)>;

            explicit MethodEventListener(Callback callback)
                : m_callback(std::move(callback)) {
            }

            void ProcessEvent(Rml::Event& e) override
            {
                if (m_callback)
                    m_callback(e);
            }

            void OnDetach(Rml::Element*) override
            {
                delete this;
            }

        private:
            Callback m_callback;
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

        static std::string TrimCopy(std::string_view s)
        {
            const auto isSpace = [](char c) {
                return c == ' ' || c == '\t' || c == '\n' || c == '\r';
            };

            while (!s.empty() && isSpace(s.front()))
                s.remove_prefix(1);
            while (!s.empty() && isSpace(s.back()))
                s.remove_suffix(1);

            return std::string(s);
        }

        static bool ParseFloat(const Rml::String& text, float& out)
        {
            const std::string trimmed = TrimCopy(text);
            if (trimmed.empty())
                return false;

            const char* begin = trimmed.c_str();
            char* end = nullptr;
            const float value = std::strtof(begin, &end);
            if (end == begin || *end != '\0')
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
            char buf[64]{};
            std::snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(value));

            std::string text(buf);
            auto dot = text.find('.');
            if (dot != std::string::npos) {
                while (!text.empty() && text.back() == '0')
                    text.pop_back();
                if (!text.empty() && text.back() == '.')
                    text.pop_back();
            }

            if (text == "-0")
                return "0";

            return text.empty() ? "0" : text;
        }

        static Rml::String ToRmlFloatString(float value)
        {
            return Rml::String(FormatFloat(value).c_str());
        }

        static std::uint64_t HashCombine(std::uint64_t seed, std::uint64_t value)
        {
            seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
            return seed;
        }

        static std::uint64_t HashFloat(float value)
        {
            static_assert(sizeof(float) == sizeof(std::uint32_t), "Unexpected float size");
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            return bits;
        }
    }

    void EditorOverlay::Init(Application& app)
    {
        m_app = &app;
        m_enabled = true;
        m_statsAccum = 0.0f;
        m_refreshAccum = 0.0f;

        m_rebuildHierarchy = true;
        m_rebuildInspector = true;
        m_refreshStats = true;
        m_inspectorEditing = false;
        m_lastSceneDigest = 0;

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
        RefreshInspector(true);
        RefreshStats();

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

        m_rebuildHierarchy = true;
        m_rebuildInspector = true;
        m_refreshStats = true;
        m_inspectorEditing = false;
        m_lastSceneDigest = 0;
    }

    void EditorOverlay::SetScene(Scene2D* scene)
    {
        m_scene = scene;
        m_selectedEntity = 0;
        m_inspectorEditing = false;
        m_lastSceneDigest = BuildSceneDigest();

        MarkHierarchyDirty();
        MarkInspectorDirty();
        MarkStatsDirty();

        KbkLog(kLogChannel, "SetScene called. scene=%p", static_cast<void*>(scene));
    }

    void EditorOverlay::SetEnabled(bool enabled)
    {
        m_enabled = enabled;

        if (!m_doc)
            return;

        if (m_enabled) {
            m_doc->Show();
            MarkHierarchyDirty();
            MarkInspectorDirty();
            MarkStatsDirty();
        }
        else {
            m_doc->Hide();
        }
    }

    void EditorOverlay::BindButtons()
    {
        if (!m_doc)
            return;

        if (auto* quit = m_doc->GetElementById("btn_quit")) {
            quit->AddEventListener("click", new MethodEventListener(std::bind(&EditorOverlay::OnQuitClicked, this, std::placeholders::_1)));
        }
        else {
            KbkWarn(kLogChannel, "Missing #btn_quit element");
        }

        if (auto* apply = m_doc->GetElementById("btn_apply")) {
            apply->AddEventListener("click", new MethodEventListener(std::bind(&EditorOverlay::OnApplyClicked, this, std::placeholders::_1)));
        }
        else {
            KbkWarn(kLogChannel, "Missing #btn_apply element");
        }

        auto bindInspectorInput = [this](Rml::ElementFormControlInput* input) {
            if (!input)
                return;
            input->AddEventListener("focus", new MethodEventListener(std::bind(&EditorOverlay::OnInspectorFocus, this, std::placeholders::_1)));
            input->AddEventListener("blur", new MethodEventListener(std::bind(&EditorOverlay::OnInspectorBlur, this, std::placeholders::_1)));
            input->AddEventListener("change", new MethodEventListener(std::bind(&EditorOverlay::OnInspectorInputChanged, this, std::placeholders::_1)));
            input->AddEventListener("input", new MethodEventListener(std::bind(&EditorOverlay::OnInspectorInputChanged, this, std::placeholders::_1)));
        };

        bindInspectorInput(m_insName);
        bindInspectorInput(m_insPosX);
        bindInspectorInput(m_insPosY);
        bindInspectorInput(m_insRot);
        bindInspectorInput(m_insScaleX);
        bindInspectorInput(m_insScaleY);
    }

    void EditorOverlay::SelectEntity(EntityID id)
    {
        if (m_selectedEntity == id)
            return;

        m_selectedEntity = id;
        m_inspectorEditing = false;

        KbkLog(kLogChannel, "Selected entity id=%u", static_cast<unsigned>(m_selectedEntity));

        MarkHierarchyDirty();
        MarkInspectorDirty();
    }

    void EditorOverlay::OnQuitClicked(Rml::Event&)
    {
        KbkLog(kLogChannel, "Rml event: click #btn_quit");
        SDL_Event evt{};
        evt.type = SDL_QUIT;
        SDL_PushEvent(&evt);
    }

    void EditorOverlay::OnApplyClicked(Rml::Event&)
    {
        KbkLog(kLogChannel, "Rml event: click #btn_apply");
        ApplyInspector();
        if (m_onApply)
            m_onApply();
    }

    void EditorOverlay::OnHierarchyEntityClicked(EntityID id, Rml::Event&)
    {
        KbkLog(kLogChannel, "Rml event: click hierarchy entity id=%u", static_cast<unsigned>(id));
        SelectEntity(id);
    }

    void EditorOverlay::OnInspectorFocus(Rml::Event& evt)
    {
        m_inspectorEditing = true;
        if (auto* target = evt.GetTargetElement()) {
            KbkLog(kLogChannel, "Rml event: focus #%s", target->GetId().c_str());
        }
    }

    void EditorOverlay::OnInspectorBlur(Rml::Event& evt)
    {
        m_inspectorEditing = false;
        if (auto* target = evt.GetTargetElement()) {
            KbkLog(kLogChannel, "Rml event: blur #%s", target->GetId().c_str());
        }
        MarkInspectorDirty();
    }

    void EditorOverlay::OnInspectorInputChanged(Rml::Event& evt)
    {
        m_inspectorEditing = true;
        if (auto* target = evt.GetTargetElement()) {
            KbkLog(kLogChannel, "Rml event: %s #%s", evt.GetType().c_str(), target->GetId().c_str());
        }
    }

    void EditorOverlay::MarkHierarchyDirty()
    {
        m_rebuildHierarchy = true;
    }

    void EditorOverlay::MarkInspectorDirty()
    {
        m_rebuildInspector = true;
    }

    void EditorOverlay::MarkStatsDirty()
    {
        m_refreshStats = true;
    }

    std::uint64_t EditorOverlay::BuildSceneDigest() const
    {
        if (!m_scene)
            return 0;

        std::uint64_t hash = 1469598103934665603ULL;

        for (const auto& e : m_scene->Entities()) {
            hash = HashCombine(hash, static_cast<std::uint64_t>(e.id));
            hash = HashCombine(hash, e.active ? 1ULL : 0ULL);
            hash = HashCombine(hash, HashFloat(e.transform.position.x));
            hash = HashCombine(hash, HashFloat(e.transform.position.y));
            hash = HashCombine(hash, HashFloat(e.transform.rotation));
            hash = HashCombine(hash, HashFloat(e.transform.scale.x));
            hash = HashCombine(hash, HashFloat(e.transform.scale.y));

            const NameComponent* name = m_scene->TryGetName(e.id);
            if (name) {
                for (char c : name->name) {
                    hash = HashCombine(hash, static_cast<std::uint64_t>(static_cast<unsigned char>(c)));
                }
            }
        }

        return hash;
    }

    void EditorOverlay::Update(float dt)
    {
        if (!m_enabled || !m_doc || !m_app)
            return;

        m_statsAccum += dt;
        m_refreshAccum += dt;

        if (m_statsAccum >= m_statsPeriod) {
            m_statsAccum = 0.0f;
            MarkStatsDirty();
        }

        if (m_refreshAccum >= m_refreshPeriod) {
            m_refreshAccum = 0.0f;

            const std::uint64_t digest = BuildSceneDigest();
            if (digest != m_lastSceneDigest) {
                m_lastSceneDigest = digest;
                MarkHierarchyDirty();
                MarkStatsDirty();
                if (!m_inspectorEditing)
                    MarkInspectorDirty();
            }
        }

        if (m_refreshStats) {
            RefreshStats();
            m_refreshStats = false;
        }

        if (m_rebuildHierarchy) {
            RefreshHierarchy();
            m_rebuildHierarchy = false;
        }

        if (m_rebuildInspector) {
            RefreshInspector(false);
            m_rebuildInspector = false;
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
        int activeCount = 0;

        for (const auto& entity : m_scene->Entities()) {
            if (!entity.active)
                continue;

            ++activeCount;

            auto button = m_doc->CreateElement("button");
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
            button->AddEventListener("click", new MethodEventListener(std::bind(&EditorOverlay::OnHierarchyEntityClicked, this, id, std::placeholders::_1)));

            if (id == m_selectedEntity) {
                button->SetClass("selected", true);
                selectionStillValid = true;
            }

            m_hierarchyList->AppendChild(std::move(button));
        }

        if (!selectionStillValid && m_selectedEntity != 0) {
            m_selectedEntity = 0;
            m_inspectorEditing = false;
            MarkInspectorDirty();
        }

        KbkLog(kLogChannel, "Hierarchy rebuild: active entities=%d selected=%u", activeCount, static_cast<unsigned>(m_selectedEntity));
    }

    void EditorOverlay::RefreshInspector(bool force)
    {
        if (!m_inspectorHint)
            return;

        if (!force && m_inspectorEditing)
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
            m_insPosX->SetValue(ToRmlFloatString(entity->transform.position.x));
        if (m_insPosY)
            m_insPosY->SetValue(ToRmlFloatString(entity->transform.position.y));
        if (m_insRot)
            m_insRot->SetValue(ToRmlFloatString(entity->transform.rotation));
        if (m_insScaleX)
            m_insScaleX->SetValue(ToRmlFloatString(entity->transform.scale.x));
        if (m_insScaleY)
            m_insScaleY->SetValue(ToRmlFloatString(entity->transform.scale.y));
    }

    void EditorOverlay::ApplyInspector()
    {
        if (!m_scene || m_selectedEntity == 0)
            return;

        auto* entity = m_scene->FindEntity(m_selectedEntity);
        if (!entity || !entity->active)
            return;

        const auto before = entity->transform;

        if (m_insName) {
            const auto nameValue = TrimCopy(m_insName->GetValue());
            if (auto* name = m_scene->TryGetName(entity->id)) {
                name->name = nameValue;
            }
            else if (!nameValue.empty()) {
                m_scene->AddName(entity->id, nameValue);
            }
        }

        float parsed = 0.0f;
        if (m_insPosX && ParseFloat(m_insPosX->GetValue(), parsed))
            entity->transform.position.x = parsed;
        if (m_insPosY && ParseFloat(m_insPosY->GetValue(), parsed))
            entity->transform.position.y = parsed;
        if (m_insRot && ParseFloat(m_insRot->GetValue(), parsed))
            entity->transform.rotation = parsed;
        if (m_insScaleX && ParseFloat(m_insScaleX->GetValue(), parsed))
            entity->transform.scale.x = parsed;
        if (m_insScaleY && ParseFloat(m_insScaleY->GetValue(), parsed))
            entity->transform.scale.y = parsed;

        KbkLog(
            kLogChannel,
            "Apply entity=%u before(pos=%.4f,%.4f rot=%.4f scale=%.4f,%.4f) after(pos=%.4f,%.4f rot=%.4f scale=%.4f,%.4f)",
            static_cast<unsigned>(entity->id),
            before.position.x,
            before.position.y,
            before.rotation,
            before.scale.x,
            before.scale.y,
            entity->transform.position.x,
            entity->transform.position.y,
            entity->transform.rotation,
            entity->transform.scale.x,
            entity->transform.scale.y);

        m_inspectorEditing = false;
        m_lastSceneDigest = BuildSceneDigest();

        MarkHierarchyDirty();
        MarkInspectorDirty();
        MarkStatsDirty();

        RefreshInspector(true);
    }

} // namespace KibakoEngine

#endif // KBK_DEBUG_BUILD
