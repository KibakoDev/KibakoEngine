#pragma once

#include "KibakoEngine/Core/Layer.h"

namespace KibakoEngine {
    class Application;
    class SpriteBatch2D;
}

namespace Rml {
    class ElementDocument;
}

class UILayer final : public KibakoEngine::Layer
{
public:
    explicit UILayer(KibakoEngine::Application& app);

    // Uses the shared RmlUI context for pixel-perfect overlays

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

private:
    KibakoEngine::Application& m_app;

    // Currently manages only the main menu UI document
    Rml::ElementDocument* m_mainMenuDoc = nullptr;
};
