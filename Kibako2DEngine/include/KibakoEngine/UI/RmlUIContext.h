// Bridges the engine renderer with an RmlUI context
#pragma once

#include <SDL2/SDL.h>
#include <RmlUi/Core.h>

#include <memory>

namespace KibakoEngine {

    class RendererD3D11;
    class RmlSystemInterface;
    class RmlRenderInterfaceD3D11;

    class RmlUIContext {
    public:
        RmlUIContext();
        ~RmlUIContext();

        // Initialize the RmlUI context once at application startup
        [[nodiscard]] bool Init(RendererD3D11& renderer,
            int width,
            int height,
            bool enableDebugger = false);
        void Shutdown();

        // Update the UI surface when the window is resized (width/height in pixels)
        void OnResize(int width, int height);

        // Forward SDL events collected by the application
        void ProcessSDLEvent(const SDL_Event& evt);

        // Advance and render the UI each frame after game updates
        void Update(float dt);
        void Render();

        // Accessors for the underlying Rml context
        Rml::Context* GetContext() { return m_context; }
        const Rml::Context* GetContext() const { return m_context; }

        // Load an RML document from disk (e.g. "assets/ui/main_menu.rml")
        Rml::ElementDocument* LoadDocument(const Rml::String& path);

    private:
        RendererD3D11* m_renderer = nullptr;
        std::unique_ptr<RmlSystemInterface>      m_systemInterface;
        std::unique_ptr<RmlRenderInterfaceD3D11> m_renderInterface;
        Rml::Context* m_context = nullptr;

        bool m_initialized = false;

        // UI resolution in native backbuffer pixels
        int m_width = 0;
        int m_height = 0;

        // Modifiers / key mapping
        int  GetKeyModifiers(const SDL_Event& evt) const;
        Rml::Input::KeyIdentifier ToRmlKey(SDL_Keycode key) const;
    };

} // namespace KibakoEngine