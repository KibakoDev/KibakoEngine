// Handles UI logic for the sandbox scene
#include "UILayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/UI/RmlUIContext.h"

#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>

#include <functional>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "UI";

    // Helper to attach C++ callbacks to RmlUI events
    class ButtonListener : public Rml::EventListener
    {
    public:
        using Callback = std::function<void(Rml::Event&)>;

        explicit ButtonListener(Callback cb)
            : m_callback(std::move(cb))
        {
        }

        void ProcessEvent(Rml::Event& event) override
        {
            if (m_callback)
                m_callback(event);
        }

        // RmlUi owns the listener, so clean up when it is detached
        void OnDetach(Rml::Element*) override
        {
            delete this;
        }

    private:
        Callback m_callback;
    };
} // namespace

UILayer::UILayer(Application& app)
    : Layer("Sandbox.UILayer")
    , m_app(app)
{
}

void UILayer::OnAttach()
{
    auto& ui = m_app.UI();

    /* Load custom fonts
    static bool s_fontsLoaded = false;
    if (!s_fontsLoaded)
    {
        if (!Rml::LoadFontFace("assets/ui/fonts/customfont.ttf")) {
            KbkWarn(kLogChannel, "Failed to load font face: Custom Font");
        }
        s_fontsLoaded = true;
    } */

    // Load the main menu document
    m_mainMenuDoc = ui.LoadDocument("assets/ui/main_menu.rml");
    if (!m_mainMenuDoc) {
        KbkError(kLogChannel, "Failed to load main_menu.rml");
        return;
    }

    // Hook the Quit button
    if (auto* quit = m_mainMenuDoc->GetElementById("btn_quit")) {
        quit->AddEventListener(
            "click",
            new ButtonListener([this](Rml::Event&) {
                KbkLog(kLogChannel, "Quit clicked");
                SDL_Event quit{};
                quit.type = SDL_QUIT;
                SDL_PushEvent(&quit);
                })
        );
    }

    m_mainMenuDoc->Show();
}

void UILayer::OnDetach()
{
    if (m_mainMenuDoc) {
        m_mainMenuDoc->Hide();
        m_mainMenuDoc = nullptr;
    }
}

void UILayer::OnUpdate(float /*dt*/)
{
}

void UILayer::OnRender(SpriteBatch2D& /*batch*/)
{
}