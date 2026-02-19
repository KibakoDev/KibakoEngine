// Integrates RmlUI with the engine renderer and input
#include "KibakoEngine/UI/RmlUIContext.h"

#include "KibakoEngine/UI/RmlSystemInterface.h"
#include "KibakoEngine/UI/RmlRenderInterfaceD3D11.h"
#include "KibakoEngine/Renderer/RendererD3D11.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"

#include <RmlUi/Debugger.h>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "RmlUI";
    }

    //===========================
    //      Construction
    //===========================

    RmlUIContext::RmlUIContext() = default;

    RmlUIContext::~RmlUIContext()
    {
        Shutdown();
    }

    //===========================
    //          Init
    //===========================

    bool RmlUIContext::Init(RendererD3D11& renderer,
        int width,
        int height,
        bool enableDebugger)
    {
        if (m_initialized)
            return true;

        m_renderer = &renderer;
        m_width = (width > 0) ? width : 1;
        m_height = (height > 0) ? height : 1;

        m_systemInterface = std::make_unique<RmlSystemInterface>();
        m_renderInterface = std::make_unique<RmlRenderInterfaceD3D11>(renderer);

        Rml::SetSystemInterface(m_systemInterface.get());
        Rml::SetRenderInterface(m_renderInterface.get());

        if (!Rml::Initialise()) {
            KbkError(kLogChannel, "Rml::Initialise() failed");
            Rml::SetSystemInterface(nullptr);
            Rml::SetRenderInterface(nullptr);
            m_systemInterface.reset();
            m_renderInterface.reset();
            m_renderer = nullptr;
            return false;
        }

        m_context = Rml::CreateContext("main", Rml::Vector2i(m_width, m_height));
        if (!m_context) {
            KbkError(kLogChannel, "Rml::CreateContext() failed");
            Rml::Shutdown();
            Rml::SetSystemInterface(nullptr);
            Rml::SetRenderInterface(nullptr);
            m_systemInterface.reset();
            m_renderInterface.reset();
            m_renderer = nullptr;
            return false;
        }

        if (!Rml::Debugger::Initialise(m_context)) {
            KbkWarn(kLogChannel, "Failed to initialize RmlUi debugger; debug font may be unavailable.");
        }

        if (!enableDebugger) {
            Rml::Debugger::SetVisible(false);
        }

        m_initialized = true;
        KbkLog(kLogChannel, "RmlUIContext initialized (%dx%d backbuffer space)", m_width, m_height);
        return true;
    }

    //===========================
    //        Shutdown
    //===========================

    void RmlUIContext::Shutdown()
    {
        const bool wasInitialized = m_initialized;

        if (m_context) {
            Rml::RemoveContext(m_context->GetName());
            m_context = nullptr;
        }

        if (m_initialized) {
            Rml::Shutdown();
            m_initialized = false;
        }

        Rml::SetSystemInterface(nullptr);
        Rml::SetRenderInterface(nullptr);

        m_renderInterface.reset();
        m_systemInterface.reset();
        m_renderer = nullptr;
        m_width = 0;
        m_height = 0;

        if (wasInitialized) {
            KbkLog(kLogChannel, "RmlUIContext shutdown");
        }
    }

    //===========================
    //         Resize
    //===========================

    void RmlUIContext::OnResize(int width, int height)
    {
        if (!m_context)
            return;

        m_width = (width > 0) ? width : 1;
        m_height = (height > 0) ? height : 1;

        m_context->SetDimensions(Rml::Vector2i(m_width, m_height));

        KbkLog(kLogChannel, "UI resized to %dx%d (backbuffer space)", m_width, m_height);
    }

    //===========================
    //       Modifiers / Keys
    //===========================

    int RmlUIContext::GetKeyModifiers(const SDL_Event& evt) const
    {
        SDL_Keymod mods;

        if (evt.type == SDL_KEYDOWN || evt.type == SDL_KEYUP)
            mods = static_cast<SDL_Keymod>(evt.key.keysym.mod);
        else
            mods = SDL_GetModState();

        int rml_mods = 0;

        if (mods & KMOD_SHIFT) rml_mods |= Rml::Input::KM_SHIFT;
        if (mods & KMOD_CTRL)  rml_mods |= Rml::Input::KM_CTRL;
        if (mods & KMOD_ALT)   rml_mods |= Rml::Input::KM_ALT;
        if (mods & KMOD_CAPS)  rml_mods |= Rml::Input::KM_CAPSLOCK;
        if (mods & KMOD_NUM)   rml_mods |= Rml::Input::KM_NUMLOCK;

        return rml_mods;
    }

    Rml::Input::KeyIdentifier RmlUIContext::ToRmlKey(SDL_Keycode key) const
    {
        using namespace Rml::Input;

        if (key >= SDLK_a && key <= SDLK_z)
            return static_cast<KeyIdentifier>(KI_A + (key - SDLK_a));
        if (key >= SDLK_0 && key <= SDLK_9)
            return static_cast<KeyIdentifier>(KI_0 + (key - SDLK_0));

        switch (key) {
        case SDLK_RETURN:    return KI_RETURN;
        case SDLK_ESCAPE:    return KI_ESCAPE;
        case SDLK_BACKSPACE: return KI_BACK;
        case SDLK_TAB:       return KI_TAB;
        case SDLK_SPACE:     return KI_SPACE;

        case SDLK_LEFT:      return KI_LEFT;
        case SDLK_RIGHT:     return KI_RIGHT;
        case SDLK_UP:        return KI_UP;
        case SDLK_DOWN:      return KI_DOWN;

        case SDLK_HOME:      return KI_HOME;
        case SDLK_END:       return KI_END;
        case SDLK_PAGEUP:    return KI_PRIOR;
        case SDLK_PAGEDOWN:  return KI_NEXT;
        case SDLK_INSERT:    return KI_INSERT;
        case SDLK_DELETE:    return KI_DELETE;

        case SDLK_F1:        return KI_F1;
        case SDLK_F2:        return KI_F2;
        case SDLK_F3:        return KI_F3;
        case SDLK_F4:        return KI_F4;
        case SDLK_F5:        return KI_F5;
        case SDLK_F6:        return KI_F6;
        case SDLK_F7:        return KI_F7;
        case SDLK_F8:        return KI_F8;
        case SDLK_F9:        return KI_F9;
        case SDLK_F10:       return KI_F10;
        case SDLK_F11:       return KI_F11;
        case SDLK_F12:       return KI_F12;

        default:             return KI_UNKNOWN;
        }
    }

    //===========================
    //       ProcessSDLEvent
    //===========================

    void RmlUIContext::ProcessSDLEvent(const SDL_Event& evt)
    {
        KBK_PROFILE_SCOPE("RmlUIContext::ProcessSDLEvent");

        if (!m_context)
            return;

        const int mods = GetKeyModifiers(evt);

        switch (evt.type) {

        case SDL_MOUSEMOTION: {
            m_context->ProcessMouseMove(evt.motion.x, evt.motion.y, mods);
            break;
        }

        case SDL_MOUSEBUTTONDOWN: {
            int button = -1;
            if (evt.button.button == SDL_BUTTON_LEFT)        button = 0;
            else if (evt.button.button == SDL_BUTTON_RIGHT)  button = 1;
            else if (evt.button.button == SDL_BUTTON_MIDDLE) button = 2;

            if (button >= 0) {
                m_context->ProcessMouseMove(evt.button.x, evt.button.y, mods);
                m_context->ProcessMouseButtonDown(button, mods);
            }
            break;
        }

        case SDL_MOUSEBUTTONUP: {
            int button = -1;
            if (evt.button.button == SDL_BUTTON_LEFT)        button = 0;
            else if (evt.button.button == SDL_BUTTON_RIGHT)  button = 1;
            else if (evt.button.button == SDL_BUTTON_MIDDLE) button = 2;

            if (button >= 0) {
                m_context->ProcessMouseMove(evt.button.x, evt.button.y, mods);
                m_context->ProcessMouseButtonUp(button, mods);
            }
            break;
        }

        case SDL_MOUSEWHEEL: {
            // Always resync hover element before scrolling.
            int mx = 0, my = 0;
            SDL_GetMouseState(&mx, &my);
            m_context->ProcessMouseMove(mx, my, mods);

            float wheelX = static_cast<float>(evt.wheel.x);
            float wheelY = static_cast<float>(evt.wheel.y);

            // SDL can report flipped wheel on some systems/settings.
            if (evt.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
                wheelX = -wheelX;
                wheelY = -wheelY;
            }

            // RmlUi: positive values are directed right and down.
            // SDL: wheel.y is typically +1 when scrolling up, so invert Y.
            m_context->ProcessMouseWheel({ wheelX, -wheelY }, mods);
            break;
        }

        case SDL_KEYDOWN: {
            if (!evt.key.repeat) {
                const Rml::Input::KeyIdentifier key_id =
                    ToRmlKey(evt.key.keysym.sym);
                m_context->ProcessKeyDown(key_id, mods);
            }
            break;
        }

        case SDL_KEYUP: {
            const Rml::Input::KeyIdentifier key_id =
                ToRmlKey(evt.key.keysym.sym);
            m_context->ProcessKeyUp(key_id, mods);
            break;
        }

        case SDL_TEXTINPUT: {
            const char* text = evt.text.text;
            for (int i = 0; text[i] != '\0'; ++i) {
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if (c >= 32 && c != 127) {
                    m_context->ProcessTextInput(
                        static_cast<Rml::Character>(c));
                }
            }
            break;
        }

        default:
            break;
        }
    }

    //===========================
    //         Update
    //===========================

    void RmlUIContext::Update(float dt)
    {
        KBK_PROFILE_SCOPE("RmlUIContext::Update");
        (void)dt; // RmlUI drives its own timing through the SystemInterface
        if (m_context)
            m_context->Update();
    }

    //===========================
    //         Render
    //===========================

    void RmlUIContext::Render()
    {
        KBK_PROFILE_SCOPE("RmlUIContext::Render");
        if (m_context)
            m_context->Render();
    }

    //===========================
    //      LoadDocument
    //===========================

    Rml::ElementDocument* RmlUIContext::LoadDocument(const Rml::String& path)
    {
        if (!m_context)
            return nullptr;

        Rml::ElementDocument* doc = m_context->LoadDocument(path);
        if (!doc) {
            KbkError(kLogChannel, "Failed to load RML document: %s", path.c_str());
            return nullptr;
        }

        return doc;
    }

} // namespace KibakoEngine