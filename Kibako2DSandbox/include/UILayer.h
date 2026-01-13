#pragma once

#include "KibakoEngine/Core/Layer.h"
#include <cstdint>

namespace KibakoEngine {
    class Application;
    class SpriteBatch2D;
    class Scene2D;
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
    void UpdateSelectionVisuals();

private:
    KibakoEngine::Application& m_app;

    Rml::ElementDocument* m_editorDoc = nullptr;
    Rml::Element* m_hierarchyList = nullptr;

    KibakoEngine::Scene2D* m_lastScene = nullptr;
    std::uint32_t m_lastSelected = 0;
};