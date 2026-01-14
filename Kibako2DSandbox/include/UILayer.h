#pragma once

#include <cstdint>

#include "KibakoEngine/Core/Layer.h"

namespace KibakoEngine {
    class Application;
    class SpriteBatch2D;
}

namespace Rml {
    class ElementDocument;
    class Element;
}

class UILayer final : public KibakoEngine::Layer
{
public:
    explicit UILayer(KibakoEngine::Application& app);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

private:
    void RebuildHierarchy();
    void ApplySelectionStyle();

private:
    KibakoEngine::Application& m_app;

    Rml::ElementDocument* m_mainMenuDoc = nullptr;
    Rml::ElementDocument* m_editorDoc = nullptr;

    Rml::Element* m_hierarchyList = nullptr;

    std::uint32_t m_selectedEntityId = 0;
};