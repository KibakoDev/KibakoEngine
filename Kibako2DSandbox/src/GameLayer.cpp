#include "GameLayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/Renderer/Camera2D.h"

#include <SDL2/SDL_scancode.h>
#include <cmath>

using namespace KibakoEngine;

namespace {
    constexpr const char* kLogChannel = "Sandbox";
    constexpr const char* kScenePath = "assets/scenes/test.scene.json";
}

GameLayer::GameLayer(Application& app)
    : Layer("Sandbox.GameLayer")
    , m_app(app)
{
}

void GameLayer::OnAttach()
{
    KBK_PROFILE_SCOPE("Sandbox.GameLayer.Attach");

    if (!m_scene.LoadFromFile(kScenePath, m_app.Assets())) {
        KbkError(kLogChannel, "Failed to load scene: %s", kScenePath);
        return;
    }

    if (auto* e = m_scene.FindByName("LeftStar"))  m_entityLeft = e->id;
    else KbkError(kLogChannel, "Entity 'LeftStar' not found");

    if (auto* e = m_scene.FindByName("RightStar")) m_entityRight = e->id;
    else KbkError(kLogChannel, "Entity 'RightStar' not found");

    // Default: debug OFF
    m_scene.SetCollisionDebugEnabled(false);

    KbkLog(kLogChannel, "GameLayer attached (scene loaded, %zu entities)", m_scene.Entities().size());
}

void GameLayer::OnDetach()
{
    KBK_PROFILE_SCOPE("Sandbox.GameLayer.Detach");

    m_scene.SetCollisionDebugEnabled(false);
    m_scene.Clear();

    m_entityLeft = 0;
    m_entityRight = 0;
    m_simTime = 0.0f;
}

void GameLayer::OnUpdate(float /*dt*/)
{
    KBK_PROFILE_SCOPE("Sandbox.GameLayer.Update");

    auto& input = m_app.InputSys();
    if (input.KeyPressed(SDL_SCANCODE_F1)) {
        ToggleCollisionDebug();
    }
}

void GameLayer::OnFixedUpdate(float fixedDt)
{
    KBK_PROFILE_SCOPE("Sandbox.GameLayer.FixedUpdate");
    FixedSimStep(fixedDt);
}

void GameLayer::OnRender(SpriteBatch2D& batch)
{
    KBK_PROFILE_SCOPE("Sandbox.GameLayer.Render");

    const Camera2D& camera = m_app.Renderer().Camera();

    // If camera rotates, conservative render (no culling rect)
    if (std::fabs(camera.GetRotation()) > 0.0001f) {
        m_scene.Render(batch);
        return;
    }

    const RectF visibleRect = RectF::FromXYWH(
        camera.GetPosition().x,
        camera.GetPosition().y,
        camera.GetViewportWidth(),
        camera.GetViewportHeight());

    m_scene.Render(batch, &visibleRect);
}

void GameLayer::ToggleCollisionDebug()
{
    const bool enabled = !m_scene.IsCollisionDebugEnabled();
    m_scene.SetCollisionDebugEnabled(enabled);
}

void GameLayer::FixedSimStep(float fixedDt)
{
    m_simTime += fixedDt;

    Entity2D* left = m_scene.FindEntity(m_entityLeft);
    Entity2D* right = m_scene.FindEntity(m_entityRight);

    if (left) {
        left->transform.position.y += 0.1f;
    }

    const auto* leftCol = m_scene.Collisions().TryGet(m_entityLeft);
    const auto* rightCol = m_scene.Collisions().TryGet(m_entityRight);

    bool hit = false;
    if (left && right && leftCol && rightCol && leftCol->circle && rightCol->circle) {
        hit = Intersects(*leftCol->circle, left->transform, *rightCol->circle, right->transform);
    }

    if (auto* spr = m_scene.Sprites().TryGet(m_entityLeft)) {
        spr->color = hit ? Color4::White() : Color4{ 0.9f, 0.9f, 0.9f, 1.0f };
    }

    if (auto* spr = m_scene.Sprites().TryGet(m_entityRight)) {
        spr->color = hit ? Color4{ 0.85f, 0.85f, 0.85f, 1.0f } : Color4{ 0.55f, 0.55f, 0.55f, 1.0f };
    }

    m_scene.Update(fixedDt);
}