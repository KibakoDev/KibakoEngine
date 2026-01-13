#pragma once

#include "KibakoEngine/Core/Layer.h"
#include <cstddef>
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
    void CloseEditorDocument();
    void ValidateSelection(KibakoEngine::Scene2D* scene);
    void RebuildHierarchy();
    void UpdateSelectionVisuals();
    std::size_t ComputeHierarchySignature(const KibakoEngine::Scene2D& scene) const;

private:
    KibakoEngine::Application& m_app;

    Rml::ElementDocument* m_editorDoc = nullptr;
    Rml::Element* m_hierarchyList = nullptr;

    KibakoEngine::Scene2D* m_lastScene = nullptr;
    std::uint32_t m_lastSelected = 0;
    std::size_t m_lastHierarchySignature = 0;
};
