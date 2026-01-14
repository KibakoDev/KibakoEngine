#include "UILayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Editor/EditorContext.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <SDL2/SDL_events.h>

#include <functional>
#include <string>
#include <utility>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "UI";

    class UiListener final : public Rml::EventListener
    {
    public:
        using Callback = std::function<void(Rml::Event&)>;

        explicit UiListener(Callback cb)
            : m_callback(std::move(cb))
        {
        }

        void ProcessEvent(Rml::Event& event) override
        {
            if (m_callback)
                m_callback(event);
        }

        void OnDetach(Rml::Element*) override
        {
            delete this;
        }

    private:
        Callback m_callback;
    };
}

UILayer::UILayer(Application& app)
    : Layer("Sandbox.UILayer")
    , m_app(app)
{
}

void UILayer::OnAttach()
{
    auto& ui = m_app.UI();

    // --- Main menu
    m_mainMenuDoc = ui.LoadDocument("assets/ui/main_menu.rml");
    if (!m_mainMenuDoc) {
        KbkError(kLogChannel, "Failed to load assets/ui/main_menu.rml");
        return;
    }

    if (auto* quit = m_mainMenuDoc->GetElementById("btn_quit")) {
        quit->AddEventListener("click",
            new UiListener([](Rml::Event&) {
                KbkLog(kLogChannel, "Quit clicked");
                SDL_Event quit{};
                quit.type = SDL_QUIT;
                SDL_PushEvent(&quit);
                })
        );
    }

    m_mainMenuDoc->Show();

    // --- Editor UI (Hierarchy)
    m_editorDoc = ui.LoadDocument("assets/ui/editor.rml");
    if (!m_editorDoc) {
        KbkError(kLogChannel, "Failed to load assets/ui/editor.rml");
        return;
    }

    m_hierarchyList = m_editorDoc->GetElementById("hierarchy_list");
    if (!m_hierarchyList) {
        KbkError(kLogChannel, "editor.rml missing id='hierarchy_list'");
        return;
    }

    RebuildHierarchy();
    m_editorDoc->Show();

    KbkLog(kLogChannel, "UILayer attached (main_menu + editor)");
}

void UILayer::OnDetach()
{
    if (m_mainMenuDoc) {
        m_mainMenuDoc->Hide();
        m_mainMenuDoc = nullptr;
    }

    if (m_editorDoc) {
        m_editorDoc->Hide();
        m_editorDoc = nullptr;
    }

    m_hierarchyList = nullptr;
    m_selectedEntityId = 0;
}

void UILayer::OnUpdate(float /*dt*/)
{
    // Keep our local selection in sync with the editor context
    const auto selected = m_app.Editor().Selected();
    if (selected != m_selectedEntityId) {
        m_selectedEntityId = selected;
        ApplySelectionStyle();
    }
}

void UILayer::OnRender(SpriteBatch2D& /*batch*/)
{
}

void UILayer::RebuildHierarchy()
{
    if (!m_editorDoc || !m_hierarchyList)
        return;

    Scene2D* scene = m_app.Editor().GetActiveScene();
    if (!scene) {
        m_hierarchyList->SetInnerRML("<div class='hint'>No active scene.</div>");
        return;
    }

    m_hierarchyList->SetInnerRML("");

    for (const Entity2D& e : scene->Entities()) {
        if (!e.active)
            continue;

        std::string label;
        if (const auto* n = scene->TryGetName(e.id); n && !n->name.empty())
            label = n->name;
        else
            label = "Entity " + std::to_string(e.id);

        Rml::Element* item = m_editorDoc->CreateElement("button");
        item->SetClass("hierarchy_item", true);
        item->SetInnerRML(label);

        // Store id as attribute to style later
        item->SetAttribute("data-entity-id", static_cast<int>(e.id));

        const std::uint32_t id = e.id;

        item->AddEventListener("click",
            new UiListener([this, id, label](Rml::Event&) {
                m_app.Editor().Select(id);
                m_selectedEntityId = id;
                KbkLog(kLogChannel, "Selected: %s (id=%u)", label.c_str(), id);
                ApplySelectionStyle();
                })
        );

        m_hierarchyList->AppendChild(item);
    }

    ApplySelectionStyle();
}

void UILayer::ApplySelectionStyle()
{
    if (!m_hierarchyList)
        return;

    const int numChildren = m_hierarchyList->GetNumChildren();
    for (int i = 0; i < numChildren; ++i) {
        Rml::Element* child = m_hierarchyList->GetChild(i);
        if (!child)
            continue;

        const auto attr = child->GetAttribute("data-entity-id");
        const int id = attr ? attr->Get<int>() : 0;

        const bool selected = (static_cast<std::uint32_t>(id) == m_selectedEntityId);
        child->SetClass("selected", selected);
    }
}