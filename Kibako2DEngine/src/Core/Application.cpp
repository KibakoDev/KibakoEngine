// KibakoEngine/Core/Application.cpp
// Drives the main application loop, window, and renderer setup
#include "KibakoEngine/Core/Application.h"

#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/GameServices.h"
#include "KibakoEngine/Core/Layer.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace KibakoEngine {

    namespace {
        constexpr const char* kLogChannel = "App";

        // Fixed timestep (simulation)
        constexpr double kFixedStep = 1.0 / 60.0; // 60 Hz
        constexpr double kMaxFrameDt = 0.25;       // clamp raw dt (250ms)
        constexpr int    kMaxSubSteps = 8;          // anti spiral-of-death

        void AnnounceBreakpointStop()
        {
            const char* reason = LastBreakpointMessage();
            KbkWarn(kLogChannel,
                "Halting main loop due to diagnostics breakpoint%s%s",
                (reason && reason[0] != '\0') ? ": " : "",
                (reason && reason[0] != '\0') ? reason : "");
        }

        std::filesystem::path GetExecutableDirSDL()
        {
            std::filesystem::path exeDir;
            char* basePath = SDL_GetBasePath();
            if (basePath) {
                exeDir = basePath;
                SDL_free(basePath);
            }
            return exeDir;
        }

        bool ExistsNoThrow(const std::filesystem::path& p)
        {
            std::error_code ec;
            return std::filesystem::exists(p, ec) && !ec;
        }

        // Finds a root that contains either:
        // - <root>/Kibako2DEngine/assets/ui/editor.rml  (engine content)
        // - <root>/assets/ui/editor.rml                (game content)
        std::filesystem::path FindContentRoot(const std::filesystem::path& start)
        {
            if (start.empty())
                return {};

            std::filesystem::path cursor = start;
            for (int i = 0; i < 8; ++i) {

                // Engine-first (your current setup)
                if (ExistsNoThrow(cursor / "Kibako2DEngine" / "assets" / "ui" / "editor.rml"))
                    return cursor / "Kibako2DEngine";

                // Game content
                if (ExistsNoThrow(cursor / "assets" / "ui" / "editor.rml"))
                    return cursor;

                if (!cursor.has_parent_path())
                    break;

                const auto parent = cursor.parent_path();
                if (parent == cursor)
                    break;

                cursor = parent;
            }

            return {};
        }
    }

    bool Application::CreateWindowSDL(int width, int height, const char* title)
    {
        KBK_PROFILE_SCOPE("CreateWindow");

        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            KbkError(kLogChannel, "SDL_Init failed: %s", SDL_GetError());
            return false;
        }

        m_window = SDL_CreateWindow(title,
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            width,
            height,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

        if (!m_window) {
            KbkError(kLogChannel, "SDL_CreateWindow failed: %s", SDL_GetError());
            SDL_Quit();
            return false;
        }

        SDL_GetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);

        SDL_SysWMinfo info{};
        SDL_VERSION(&info.version);
        if (SDL_GetWindowWMInfo(m_window, &info) == SDL_FALSE) {
            KbkError(kLogChannel, "SDL_GetWindowWMInfo failed: %s", SDL_GetError());
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
            SDL_Quit();
            return false;
        }

        m_hwnd = info.info.win.window;
        KBK_ASSERT(m_hwnd != nullptr, "SDL window did not provide a valid HWND");
        return true;
    }

    void Application::DestroyWindowSDL()
    {
        KBK_PROFILE_SCOPE("DestroyWindow");

        if (m_window) {
            SDL_DestroyWindow(m_window);
            m_window = nullptr;
        }
        SDL_Quit();

        m_hwnd = nullptr;
        m_hasPendingResize = false;
        m_fullscreen = false;
        m_pendingWidth = 0;
        m_pendingHeight = 0;
        m_windowedWidth = 0;
        m_windowedHeight = 0;
    }

    void Application::HandleResize()
    {
        KBK_PROFILE_SCOPE("HandleResize");

        if (!m_window)
            return;

        int drawableW = 0;
        int drawableH = 0;
        SDL_GetWindowSizeInPixels(m_window, &drawableW, &drawableH);
        if (drawableW <= 0 || drawableH <= 0)
            return;

        m_pendingWidth = drawableW;
        m_pendingHeight = drawableH;
        m_hasPendingResize = true;
    }

    void Application::ResolvePaths()
    {
        // Executable dir
        m_executableDir = GetExecutableDirSDL();

        if (m_executableDir.empty()) {
            std::error_code ec;
            auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                m_executableDir = cwd;
        }

        // Content root
        m_contentRoot = FindContentRoot(m_executableDir);

        if (m_contentRoot.empty()) {
            std::error_code ec;
            auto cwd = std::filesystem::current_path(ec);
            if (!ec)
                m_contentRoot = FindContentRoot(cwd);
        }

        if (m_contentRoot.empty()) {
            m_contentRoot = m_executableDir;
            KbkWarn(kLogChannel, "Content root not found. Using fallback: %s",
                m_contentRoot.string().c_str());
        }
        else {
            KbkLog(kLogChannel, "Content root resolved: %s", m_contentRoot.string().c_str());
        }
    }

    bool Application::Init(int width, int height, const char* title)
    {
        KBK_PROFILE_SCOPE("AppInit");

        if (m_running)
            return true;

        if (!CreateWindowSDL(width, height, title))
            return false;

        ResolvePaths();

        SDL_GetWindowSizeInPixels(m_window, &m_width, &m_height);
        m_pendingWidth = m_width;
        m_pendingHeight = m_height;
        KbkLog(kLogChannel, "Drawable size: %dx%d", m_width, m_height);

        if (!m_renderer.Init(m_hwnd,
            static_cast<std::uint32_t>(m_width),
            static_cast<std::uint32_t>(m_height))) {
            Shutdown();
            return false;
        }

        m_assets.Init(m_renderer.GetDevice());
        KbkLog(kLogChannel, "AssetManager initialized");

        GameServices::Init();

        const bool enableUiDebugger =
#ifndef NDEBUG
            true;
#else
            false;
#endif

        if (!m_ui.Init(m_renderer, m_width, m_height, enableUiDebugger)) {
            KbkError(kLogChannel, "Failed to initialize RmlUI");
            Shutdown();
            return false;
        }

#if KBK_DEBUG_BUILD
        // IMPORTANT: init overlay AFTER RmlUIContext is initialized.
        m_editorOverlay.Init(*this);
#endif

        m_running = true;
        m_fullscreen = (SDL_GetWindowFlags(m_window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0u;
        return true;
    }

    void Application::Shutdown()
    {
        KBK_PROFILE_SCOPE("AppShutdown");

        if (!m_running)
            return;

        for (Layer* layer : m_layers) {
            if (layer)
                layer->OnDetach();
        }
        m_layers.clear();

#if KBK_DEBUG_BUILD
        m_editorOverlay.Shutdown();
#endif

        m_assets.Shutdown();
        GameServices::Shutdown();
        m_ui.Shutdown();

        m_renderer.Shutdown();
        DestroyWindowSDL();

        Profiler::Flush();
        m_running = false;
    }

    bool Application::PumpEvents()
    {
        KBK_PROFILE_SCOPE("PumpEvents");

        if (!m_running)
            return false;

        if (HasBreakpointRequest())
            return false;

        Profiler::BeginFrame();

        m_input.BeginFrame();
        m_time.Tick();

        SDL_Event evt{};
        while (SDL_PollEvent(&evt) != 0) {

            switch (evt.type) {
            case SDL_QUIT:
                return false;

            case SDL_WINDOWEVENT:
                switch (evt.window.event) {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                case SDL_WINDOWEVENT_RESIZED:
                case SDL_WINDOWEVENT_MAXIMIZED:
                case SDL_WINDOWEVENT_RESTORED:
#if defined(SDL_WINDOWEVENT_DISPLAY_SCALE_CHANGED)
                case SDL_WINDOWEVENT_DISPLAY_SCALE_CHANGED:
#endif
                    HandleResize();
                    break;
                default:
                    break;
                }
                break;

            case SDL_KEYDOWN:
                if (!evt.key.repeat &&
                    (evt.key.keysym.sym == SDLK_RETURN || evt.key.keysym.sym == SDLK_KP_ENTER) &&
                    (evt.key.keysym.mod & KMOD_ALT)) {
                    ToggleFullscreen();
                }

#if KBK_DEBUG_BUILD
                if (evt.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    return false;
#endif
                break;

            default:
                break;
            }

            m_input.HandleEvent(evt);
            m_ui.ProcessSDLEvent(evt);

            if (HasBreakpointRequest())
                return false;
        }

        m_input.AfterEvents();
        ApplyPendingResize();
        return true;
    }

    void Application::ApplyPendingResize()
    {
        if (!m_hasPendingResize)
            return;

        m_hasPendingResize = false;

        const int newWidth = m_pendingWidth;
        const int newHeight = m_pendingHeight;

        if (newWidth <= 0 || newHeight <= 0)
            return;

        if (newWidth == m_width && newHeight == m_height)
            return;

        m_width = newWidth;
        m_height = newHeight;

        if (!m_fullscreen && m_window) {
            SDL_GetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
        }

        KbkLog(kLogChannel, "Resize -> %dx%d", m_width, m_height);

        m_renderer.OnResize(
            static_cast<std::uint32_t>(m_width),
            static_cast<std::uint32_t>(m_height));

        m_ui.OnResize(m_width, m_height);
    }

    void Application::ToggleFullscreen()
    {
        if (!m_window)
            return;

        if (!m_fullscreen) {
            SDL_GetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
            if (SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
                KbkError(kLogChannel, "SDL_SetWindowFullscreen failed: %s", SDL_GetError());
                return;
            }
            m_fullscreen = true;
        }
        else {
            if (SDL_SetWindowFullscreen(m_window, 0) != 0) {
                KbkError(kLogChannel, "SDL_SetWindowFullscreen failed: %s", SDL_GetError());
                return;
            }
            m_fullscreen = false;

            if (m_windowedWidth > 0 && m_windowedHeight > 0) {
                SDL_SetWindowSize(m_window, m_windowedWidth, m_windowedHeight);
            }
        }

        HandleResize();
    }

    void Application::BeginFrame(const float clearColor[4])
    {
        KBK_PROFILE_SCOPE("BeginFrame");
        m_renderer.BeginFrame(clearColor);
    }

    void Application::EndFrame(bool waitForVSync)
    {
        KBK_PROFILE_SCOPE("EndFrame");
        m_renderer.EndFrame(waitForVSync);
        m_input.EndFrame();
    }

    void Application::Run(const float clearColor[4], bool waitForVSync)
    {
        KBK_ASSERT(m_running, "Run() called before Init()");

        double accumulator = 0.0;

        while (PumpEvents()) {

            if (ConsumeBreakpointRequest()) {
                AnnounceBreakpointStop();
                break;
            }

            KBK_PROFILE_FRAME("Frame");

            double rawDt = m_time.DeltaSeconds();
            if (rawDt < 0.0) rawDt = 0.0;
            if (rawDt > kMaxFrameDt) rawDt = kMaxFrameDt;

            GameServices::Update(rawDt);

            const double scaledDt = GameServices::GetScaledDeltaTime();
            accumulator += scaledDt;

            int subSteps = 0;
            while (accumulator >= kFixedStep && subSteps < kMaxSubSteps) {

                const float fdt = static_cast<float>(kFixedStep);

                for (Layer* layer : m_layers) {
                    if (layer)
                        layer->OnFixedUpdate(fdt);
                }

                accumulator -= kFixedStep;
                ++subSteps;
            }

            if (subSteps == kMaxSubSteps) {
                accumulator = 0.0;
            }

            const float frameDt = static_cast<float>(scaledDt);

            for (Layer* layer : m_layers) {
                if (layer)
                    layer->OnUpdate(frameDt);
            }

            BeginFrame(clearColor);

            SpriteBatch2D& batch = m_renderer.Batch();
            batch.Begin(m_renderer.Camera().GetViewProjectionT());

            for (Layer* layer : m_layers) {
                if (layer)
                    layer->OnRender(batch);
            }

#if KBK_DEBUG_BUILD
            m_editorOverlay.Update(frameDt);
#endif

            m_ui.Update(frameDt);
            m_ui.Render();

            batch.End();
            EndFrame(waitForVSync);

            if (ConsumeBreakpointRequest()) {
                AnnounceBreakpointStop();
                break;
            }
        }

        if (ConsumeBreakpointRequest()) {
            AnnounceBreakpointStop();
        }
    }

#if KBK_DEBUG_BUILD
    void Application::SetEditorScene(Scene2D* scene)
    {
        m_editorOverlay.SetScene(scene);
    }
#endif

    void Application::PushLayer(Layer* layer)
    {
        if (!layer)
            return;

        m_layers.push_back(layer);
        layer->OnAttach();
    }

    void Application::PopLayer(Layer* layer)
    {
        if (!layer)
            return;

        auto it = std::find(m_layers.begin(), m_layers.end(), layer);
        if (it != m_layers.end()) {
            (*it)->OnDetach();
            m_layers.erase(it);
        }
    }

} // namespace KibakoEngine