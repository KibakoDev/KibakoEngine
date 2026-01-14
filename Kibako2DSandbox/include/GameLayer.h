#pragma once

#include <cstdint>

#include "KibakoEngine/Core/Layer.h"
#include "KibakoEngine/Scene/Scene2D.h"

namespace KibakoEngine {
    class Application;
    class SpriteBatch2D;
}

class GameLayer final : public KibakoEngine::Layer
{
public:
    explicit GameLayer(KibakoEngine::Application& app);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

    // Expose the scene for UI/editor wiring (Hierarchy / Inspector)
    KibakoEngine::Scene2D& Scene() { return m_scene; }
    const KibakoEngine::Scene2D& Scene() const { return m_scene; }

private:
    void ResolveEntityIDsFromNames();
    void UpdateScene(float dt);
    void RenderCollisionDebug(KibakoEngine::SpriteBatch2D& batch);

private:
    KibakoEngine::Application& m_app;

    KibakoEngine::Scene2D m_scene;

    std::uint32_t m_entityLeft = 0;
    std::uint32_t m_entityRight = 0;

    bool  m_showCollisionDebug = false;
    bool  m_lastCollision = false;
    float m_time = 0.0f;
};