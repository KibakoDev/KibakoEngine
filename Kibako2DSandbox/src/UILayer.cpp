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

#include <functional>
#include <string>
#include <utility>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "EditorUI";

    class UiListener final : public Rml::EventListener
    {
    public:
        using Callback = std::function<void(Rml::Event&)>;

        explicit UiListener(Callback cb) : m_cb(std::move(cb)) {}
        void ProcessEvent(Rml::Event& e) override { if (m_cb) m_cb(e); }
        void OnDetach(Rml::Element*) override { delete this; }

    private:
        Callback m_cb;
    };

    static std::string MakeEntityLabel(const Scene2D& scene, const Entity2D& e)
    {
        if (const auto* n = scene.TryGetName(e.id)) {
            if (!n->name.empty())
                return n->name;
        }
        return "Entity " + std::to_string(e.id);
    }
}

UILayer::UILayer(Application& app)
    : Layer("Sandbox.EditorUI")
    , m_app(app)
{
}

void UILayer::OnAttach()
{
    auto& ui = m_app.UI();

    m_editorDoc = ui.LoadDocument("assets/ui/editor.rml");
    if (!m_editorDoc) {
        KbkError(kLogChannel, "Failed to load assets/ui/editor.rml");
        return;
    }

    m_hierarchyList = m_editorDoc->GetElementById("hierarchy_list");
    if (!m_hierarchyList) {
        KbkError(kLogChannel, "editor.rml missing element id='hierarchy_list'");
        return;
    }

    m_editorDoc->Show();
    KbkLog(kLogChannel, "Editor UI attached");
}

void UILayer::OnDetach()
{
    if (m_editorDoc) {
        m_editorDoc->Hide();
        m_editorDoc = nullptr;
    }

    m_hierarchyList = nullptr;
    m_lastScene = nullptr;
    m_lastSelected = 0;
}

void UILayer::OnUpdate(float)
{
    Scene2D* scene = m_app.Editor().GetActiveScene();

    // Rebuild when scene becomes available or changes
    if (scene != m_lastScene) {
        m_lastScene = scene;
        RebuildHierarchy();
        m_lastSelected = m_app.Editor().Selected();
        UpdateSelectionVisuals();
    }

    // Update highlight when selection changes
    const std::uint32_t selected = m_app.Editor().Selected();
    if (selected != m_lastSelected) {
        m_lastSelected = selected;
        UpdateSelectionVisuals();
    }
}

void UILayer::OnRender(SpriteBatch2D&)
{
}

void UILayer::RebuildHierarchy()
{
    if (!m_editorDoc || !m_hierarchyList)
        return;

    m_hierarchyList->SetInnerRML("");

    Scene2D* scene = m_app.Editor().GetActiveScene();
    if (!scene) {
        KbkWarn(kLogChannel, "No active scene (hierarchy empty)");
        return;
    }

    for (const Entity2D& e : scene->Entities()) {
        if (!e.active)
            continue;

        const std::string label = MakeEntityLabel(*scene, e);

        // RmlUI uses ElementPtr (unique_ptr)
        Rml::ElementPtr item = m_editorDoc->CreateElement("button");
        Rml::Element* raw = item.get();

        raw->SetClass("hierarchy_item", true);
        raw->SetAttribute("data-entity-id", static_cast<int>(e.id));
        raw->SetInnerRML(label);

        const std::uint32_t id = e.id;

        raw->AddEventListener("click",
            new UiListener([this, id](Rml::Event&) {
                m_app.Editor().Select(id);
                })
        );

        // AppendChild takes ownership (ElementPtr)
        m_hierarchyList->AppendChild(std::move(item));
    }
}

void UILayer::UpdateSelectionVisuals()
{
    if (!m_hierarchyList)
        return;

    const std::uint32_t selected = m_app.Editor().Selected();

    const int count = m_hierarchyList->GetNumChildren();
    for (int i = 0; i < count; ++i) {
        Rml::Element* child = m_hierarchyList->GetChild(i);
        if (!child)
            continue;

        const int idAttr = child->GetAttribute<int>("data-entity-id", 0);
        const bool isSel = (selected != 0 && static_cast<std::uint32_t>(idAttr) == selected);
        child->SetClass("selected", isSel);
    }
}