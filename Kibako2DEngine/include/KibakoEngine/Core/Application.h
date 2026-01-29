// Central application class that handles windowing, rendering, and layer lifecycle
#pragma once

#include <cstdint>
#include <vector>

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Input.h"
#include "KibakoEngine/Core/Time.h"
#include "KibakoEngine/Renderer/RendererD3D11.h"
#include "KibakoEngine/Resources/AssetManager.h"
#include "KibakoEngine/UI/RmlUIContext.h"

#if KBK_DEBUG_BUILD
#include "KibakoEngine/UI/EditorOverlay.h"
#endif

struct SDL_Window;
struct HWND__;
using HWND = HWND__*;

namespace KibakoEngine {

    class Layer;

#if KBK_DEBUG_BUILD
    class Scene2D;
#endif

    class Application
    {
    public:
        Application() = default;
        ~Application() = default;

        [[nodiscard]] bool Init(int width, int height, const char* title);
        void Shutdown();

        [[nodiscard]] bool PumpEvents();
        void BeginFrame(const float clearColor[4]);
        void EndFrame(bool waitForVSync = true);
        void Run(const float clearColor[4], bool waitForVSync = true);

        [[nodiscard]] RendererD3D11& Renderer() { return m_renderer; }
        [[nodiscard]] const RendererD3D11& Renderer() const { return m_renderer; }

        [[nodiscard]] Time& TimeSys() { return m_time; }
        [[nodiscard]] const Time& TimeSys() const { return m_time; }

        [[nodiscard]] Input& InputSys() { return m_input; }
        [[nodiscard]] const Input& InputSys() const { return m_input; }

        [[nodiscard]] AssetManager& Assets() { return m_assets; }
        [[nodiscard]] const AssetManager& Assets() const { return m_assets; }

        // Native backbuffer dimensions in pixels
        [[nodiscard]] int Width() const { return m_width; }
        [[nodiscard]] int Height() const { return m_height; }

        [[nodiscard]] RmlUIContext& UI() { return m_ui; }
        [[nodiscard]] const RmlUIContext& UI() const { return m_ui; }

#if KBK_DEBUG_BUILD
        // Provide the scene currently shown/edited so the engine overlay can display stats.
        void SetDebugScene(Scene2D* scene) { m_editorUI.SetScene(scene); }
#endif

        void PushLayer(Layer* layer);
        void PopLayer(Layer* layer);

    private:
        bool CreateWindowSDL(int width, int height, const char* title);
        void DestroyWindowSDL();
        void HandleResize();
        void ApplyPendingResize();
        void ToggleFullscreen();

    private:
        SDL_Window* m_window = nullptr;
        HWND        m_hwnd = nullptr;

        int  m_width = 0;
        int  m_height = 0;
        int  m_pendingWidth = 0;
        int  m_pendingHeight = 0;
        int  m_windowedWidth = 0;
        int  m_windowedHeight = 0;
        bool m_hasPendingResize = false;
        bool m_fullscreen = false;
        bool m_running = false;

        RendererD3D11 m_renderer;
        Time          m_time;
        Input         m_input;
        AssetManager  m_assets;

        RmlUIContext m_ui;

#if KBK_DEBUG_BUILD
        EditorOverlay m_editorUI;
#endif

        std::vector<Layer*> m_layers;
    };

} // namespace KibakoEngine