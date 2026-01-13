// Gameplay layer used by the sandbox application
#pragma once

#include <cstdint>

#include "KibakoEngine/Core/Layer.h"
#include "KibakoEngine/Renderer/SpriteTypes.h"
#include "KibakoEngine/Renderer/Texture2D.h"
#include "KibakoEngine/Scene/Scene2D.h"
#include "KibakoEngine/Collision/Collision2D.h"

namespace KibakoEngine {
    class Application;
}

class GameLayer final : public KibakoEngine::Layer
{
public:
    explicit GameLayer(KibakoEngine::Application& app);

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(float dt) override;
    void OnRender(KibakoEngine::SpriteBatch2D& batch) override;

private:
    // Scene update and debug rendering helpers
    void UpdateScene(float dt);
    void RenderCollisionDebug(KibakoEngine::SpriteBatch2D& batch);

private:
    KibakoEngine::Application& m_app;

    // Gameplay state
    KibakoEngine::Scene2D      m_scene;
    std::uint32_t              m_entityLeft = 0;
    std::uint32_t              m_entityRight = 0;

    KibakoEngine::Texture2D* m_starTexture = nullptr;

    bool  m_showCollisionDebug = false;
    bool  m_lastCollision = false;
    float m_time = 0.0f;
};
