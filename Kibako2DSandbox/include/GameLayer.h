// Gameplay layer used by the sandbox application
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

    void OnUpdate(float dt) override;          // variable
    void OnFixedUpdate(float fixedDt) override; // fixed
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

    // ---- Editor access
    KibakoEngine::Scene2D& GetScene() { return m_scene; }
    const KibakoEngine::Scene2D& GetScene() const { return m_scene; }

private:
    void FixedSimStep(float fixedDt);

private:
    KibakoEngine::Application& m_app;
    KibakoEngine::Scene2D m_scene;

    std::uint32_t m_entityLeft = 0;
    std::uint32_t m_entityRight = 0;

    bool  m_showCollisionDebug = false;

    // Sim time should advance in fixed step
    float m_simTime = 0.0f;
};