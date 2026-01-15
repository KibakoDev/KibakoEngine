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
    UILayer(KibakoEngine::Application& app, KibakoEngine::Scene2D& scene);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

private:
    void RebuildHierarchyUI();
    void SetSelectedEntity(std::uint32_t id);

    void RefreshInspectorFromSelection();
    void ApplyInspectorToSelection();

private:
    KibakoEngine::Application& m_app;
    KibakoEngine::Scene2D& m_scene;

    Rml::ElementDocument* m_editorDoc = nullptr;
    Rml::Element* m_hierarchyList = nullptr;
    Rml::Element* m_statsEntities = nullptr;
    Rml::Element* m_statsFps = nullptr;


    std::uint32_t m_selectedEntityId = 0;
};