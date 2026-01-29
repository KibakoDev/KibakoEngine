// Entry point for the KibakoEngine 2D sandbox
#ifndef SDL_MAIN_HANDLED
#   define SDL_MAIN_HANDLED
#endif

#ifndef NOMINMAX
#   define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#endif

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "GameLayer.h"

using namespace KibakoEngine;

int main()
{
    Application app;
    if (!app.Init(960, 540, "KibakoEngine Sandbox")) {
        KbkError("Sandbox", "Failed to initialize Application");
        return 1;
    }

    GameLayer gameLayer(app);

    app.PushLayer(&gameLayer);

    const float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    app.Run(clearColor, true);

    app.Shutdown();
    return 0;
}