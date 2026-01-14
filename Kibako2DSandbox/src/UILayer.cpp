// Handles UI logic for the sandbox scene (Editor: Hierarchy + Inspector + Apply)
#include "UILayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <SDL2/SDL_events.h>

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/Elements/ElementFormControlInput.h>

#include <functional>
#include <string>
#include <cstdlib>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "UI";

    class ButtonListener : public Rml::EventListener
    {
    public:
        using Callback = std::function<void(Rml::Event&)>;
        explicit ButtonListener(Callback cb) : m_callback(std::move(cb)) {}

        void ProcessEvent(Rml::Event& event) override { if (m_callback) m_callback(event); }
        void OnDetach(Rml::Element*) override { delete this; }

    private:
        Callback m_callback;
    };

    static std::string EntityDisplayName(const Scene2D& scene, EntityID id)
    {
        if (const auto* name = scene.Names().TryGet(id)) {
            if (!name->name.empty())
                return name->name;
        }
        return "Entity " + std::to_string(id);
    }

    static Rml::ElementFormControlInput* GetInput(Rml::ElementDocument* doc, const char* id)
    {
        if (!doc) return nullptr;
        Rml::Element* el = doc->GetElementById(id);
        return rmlui_dynamic_cast<Rml::ElementFormControlInput*>(el);
    }

    static void SetInputValue(Rml::ElementDocument* doc, const char* id, const std::string& v)
    {
        if (auto* in = GetInput(doc, id)) in->SetValue(v);
    }

    static std::string GetInputValue(Rml::ElementDocument* doc, const char* id)
    {
        if (auto* in = GetInput(doc, id)) return in->GetValue().c_str();
        return {};
    }

    static float ParseFloat(const std::string& s, float fallback)
    {
        if (s.empty()) return fallback;
        char* end = nullptr;
        const float v = std::strtof(s.c_str(), &end);
        if (end == s.c_str()) return fallback;
        return v;
    }
}

UILayer::UILayer(Application& app, Scene2D& scene)
    : Layer("Sandbox.UILayer")
    , m_app(app)
    , m_scene(scene)
{
}

void UILayer::OnAttach()
{
    auto& ui = m_app.UI();

    m_editorDoc = ui.LoadDocument("assets/ui/editor.rml");
    if (!m_editorDoc) {
        KbkError(kLogChannel, "Failed to load editor.rml");
        return;
    }

    m_hierarchyList = m_editorDoc->GetElementById("hierarchy_list");
    if (!m_hierarchyList) {
        KbkWarn(kLogChannel, "Missing #hierarchy_list element");
    }

    if (auto* quit = m_editorDoc->GetElementById("btn_quit")) {
        quit->AddEventListener("click",
            new ButtonListener([](Rml::Event&) {
                SDL_Event evt{};
                evt.type = SDL_QUIT;
                SDL_PushEvent(&evt);
                })
        );
    }

    if (auto* apply = m_editorDoc->GetElementById("btn_apply")) {
        apply->AddEventListener("click",
            new ButtonListener([this](Rml::Event&) {
                ApplyInspectorToSelection();
                })
        );
    }

    RebuildHierarchyUI();
    RefreshInspectorFromSelection(); // show hint by default
    m_editorDoc->Show();

    ui.GetContext()->Update();
}

void UILayer::OnDetach()
{
    m_hierarchyList = nullptr;
    m_selectedEntityId = 0;

    if (m_editorDoc) {
        m_editorDoc->Hide();
        m_editorDoc = nullptr;
    }
}

void UILayer::OnUpdate(float /*dt*/)
{
}

void UILayer::OnRender(SpriteBatch2D& /*batch*/)
{
}

void UILayer::SetSelectedEntity(std::uint32_t id)
{
    m_selectedEntityId = id;

    // Update hierarchy selection visuals
    if (m_hierarchyList) {
        const int count = m_hierarchyList->GetNumChildren();
        for (int i = 0; i < count; ++i) {
            if (auto* child = m_hierarchyList->GetChild(i)) {
                const Rml::String eid = child->GetAttribute<Rml::String>("data-entity", "");
                const std::uint32_t childId =
                    eid.empty() ? 0u : static_cast<std::uint32_t>(std::stoul(eid.c_str()));
                child->SetClass("selected", childId == m_selectedEntityId);
            }
        }
    }

    RefreshInspectorFromSelection();
    KbkLog(kLogChannel, "Selected entity id=%u", m_selectedEntityId);
}

void UILayer::RefreshInspectorFromSelection()
{
    if (!m_editorDoc)
        return;

    auto* hint = m_editorDoc->GetElementById("inspector_hint");

    // No selection => show hint + clear fields
    if (m_selectedEntityId == 0 || !m_scene.FindEntity(m_selectedEntityId)) {
        if (hint) hint->SetProperty("display", "block");

        SetInputValue(m_editorDoc, "ins_name", "");
        SetInputValue(m_editorDoc, "ins_pos_x", "0");
        SetInputValue(m_editorDoc, "ins_pos_y", "0");
        SetInputValue(m_editorDoc, "ins_rot", "0");
        SetInputValue(m_editorDoc, "ins_scale_x", "1");
        SetInputValue(m_editorDoc, "ins_scale_y", "1");
        return;
    }

    // Have selection => hide hint + fill fields
    if (hint) hint->SetProperty("display", "none");

    const Entity2D* e = m_scene.FindEntity(m_selectedEntityId);
    const Transform2D& t = e->transform;

    std::string name = EntityDisplayName(m_scene, e->id);

    SetInputValue(m_editorDoc, "ins_name", name);
    SetInputValue(m_editorDoc, "ins_pos_x", std::to_string(t.position.x));
    SetInputValue(m_editorDoc, "ins_pos_y", std::to_string(t.position.y));
    SetInputValue(m_editorDoc, "ins_rot", std::to_string(t.rotation));
    SetInputValue(m_editorDoc, "ins_scale_x", std::to_string(t.scale.x));
    SetInputValue(m_editorDoc, "ins_scale_y", std::to_string(t.scale.y));
}

void UILayer::ApplyInspectorToSelection()
{
    if (!m_editorDoc || m_selectedEntityId == 0)
        return;

    Entity2D* e = m_scene.FindEntity(m_selectedEntityId);
    if (!e) return;

    // Read fields
    const std::string name = GetInputValue(m_editorDoc, "ins_name");

    const float px = ParseFloat(GetInputValue(m_editorDoc, "ins_pos_x"), e->transform.position.x);
    const float py = ParseFloat(GetInputValue(m_editorDoc, "ins_pos_y"), e->transform.position.y);
    const float rot = ParseFloat(GetInputValue(m_editorDoc, "ins_rot"), e->transform.rotation);
    const float sx = ParseFloat(GetInputValue(m_editorDoc, "ins_scale_x"), e->transform.scale.x);
    const float sy = ParseFloat(GetInputValue(m_editorDoc, "ins_scale_y"), e->transform.scale.y);

    // Apply to entity
    e->transform.position = { px, py };
    e->transform.rotation = rot;
    e->transform.scale = { sx, sy };

    // Apply name component
    if (!name.empty()) {
        m_scene.AddName(e->id, name);
        RebuildHierarchyUI(); // refresh labels
        SetSelectedEntity(e->id); // keep selection & refresh inspector
    }

    KbkLog(kLogChannel, "Apply -> id=%u (pos=%.2f,%.2f rot=%.2f scale=%.2f,%.2f)",
        e->id, px, py, rot, sx, sy);
}

void UILayer::RebuildHierarchyUI()
{
    if (!m_hierarchyList || !m_editorDoc)
        return;

    m_hierarchyList->SetInnerRML("");

    for (const auto& e : m_scene.Entities()) {
        if (!e.active)
            continue;

        const std::string label = EntityDisplayName(m_scene, e.id);

        Rml::ElementPtr btn = m_editorDoc->CreateElement("button");
        if (!btn)
            continue;

        btn->SetClass("entity", true);
        btn->SetAttribute("data-entity", std::to_string(e.id).c_str());
        btn->SetInnerRML(label.c_str());

        btn->AddEventListener("click",
            new ButtonListener([this](Rml::Event& ev) {
                if (auto* el = ev.GetCurrentElement()) {
                    const Rml::String eid = el->GetAttribute<Rml::String>("data-entity", "");
                    if (!eid.empty())
                        SetSelectedEntity(static_cast<std::uint32_t>(std::stoul(eid.c_str())));
                }
                })
        );

        m_hierarchyList->AppendChild(std::move(btn));
    }

    if (m_selectedEntityId != 0)
        SetSelectedEntity(m_selectedEntityId);
}