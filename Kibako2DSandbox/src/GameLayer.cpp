// Handles gameplay logic for the sandbox scene
#include "GameLayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/Renderer/DebugDraw2D.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "Sandbox";
    constexpr int   kDebugDrawLayer = 1000;
    constexpr float kColliderThickness = 2.0f;
}

GameLayer::GameLayer(Application& app)
    : Layer("Sandbox.GameLayer")
    , m_app(app)
{
}

void GameLayer::OnAttach()
{
    KBK_PROFILE_SCOPE("GameLayerAttach");

    const bool ok = m_scene.LoadFromFile(
        "assets/scenes/test.scene.json",
        m_app.Assets());

    if (!ok) {
        KbkError(kLogChannel, "Failed to load scene: assets/scenes/test.scene.json");
        return;
    }

    if (auto* e = m_scene.FindByName("LeftStar"))
        m_entityLeft = e->id;
    else
        KbkError(kLogChannel, "Entity 'LeftStar' not found");

    if (auto* e = m_scene.FindByName("RightStar"))
        m_entityRight = e->id;
    else
        KbkError(kLogChannel, "Entity 'RightStar' not found");

    KbkLog(kLogChannel,
        "GameLayer attached (scene loaded, %zu entities)",
        m_scene.Entities().size());
}

void GameLayer::OnDetach()
{
    KBK_PROFILE_SCOPE("GameLayerDetach");

    m_scene.Clear();
    m_entityLeft = 0;
    m_entityRight = 0;

    m_showCollisionDebug = false;
    m_lastCollision = false;
    m_simTime = 0.0f;
}

void GameLayer::OnUpdate(float /*dt*/)
{
    KBK_PROFILE_SCOPE("GameLayerUpdate");

    // Variable-step: toggles / app-level inputs
    auto& input = m_app.InputSys();

    if (input.KeyPressed(SDL_SCANCODE_F1))
        m_showCollisionDebug = !m_showCollisionDebug;
}

void GameLayer::OnFixedUpdate(float fixedDt)
{
    KBK_PROFILE_SCOPE("GameLayerFixedUpdate");
    FixedSimStep(fixedDt);
}

void GameLayer::OnRender(SpriteBatch2D& batch)
{
    KBK_PROFILE_SCOPE("GameLayerRender");

    m_scene.Render(batch);

    if (m_showCollisionDebug)
        RenderCollisionDebug(batch);

    if (auto* e = m_scene.FindEntity(m_entityLeft)) {
        e->transform.position.y += 0.1f;
    }
}

void GameLayer::FixedSimStep(float fixedDt)
{
    // Deterministic sim time
    m_simTime += fixedDt;

    Entity2D* left = m_scene.FindEntity(m_entityLeft);
    Entity2D* right = m_scene.FindEntity(m_entityRight);

    const CollisionComponent2D* leftCol = m_scene.Collisions().TryGet(m_entityLeft);
    const CollisionComponent2D* rightCol = m_scene.Collisions().TryGet(m_entityRight);

    bool hit = false;
    if (left && right && leftCol && rightCol && leftCol->circle && rightCol->circle) {
        hit = Intersects(*leftCol->circle, left->transform,
            *rightCol->circle, right->transform);
    }

    if (auto* spr = m_scene.Sprites().TryGet(m_entityLeft)) {
        spr->color = hit ? Color4::White()
            : Color4{ 0.9f, 0.9f, 0.9f, 1.0f };
    }

    if (auto* spr = m_scene.Sprites().TryGet(m_entityRight)) {
        spr->color = hit ? Color4{ 0.85f, 0.85f, 0.85f, 1.0f }
        : Color4{ 0.55f, 0.55f, 0.55f, 1.0f };
    }

    m_lastCollision = hit;

    // If Scene2D::Update becomes "simulation update", it belongs here
    m_scene.Update(fixedDt);
}

void GameLayer::RenderCollisionDebug(SpriteBatch2D& batch)
{
    const Color4 circleHit = Color4::White();
    const Color4 circleIdle = Color4{ 0.7f, 0.7f, 0.7f, 1.0f };
    const Color4 crossColor = Color4::White();

    for (const Entity2D& e : m_scene.Entities()) {
        if (!e.active)
            continue;

        const Transform2D& t = e.transform;
        const Color4 circleColor = m_lastCollision ? circleHit : circleIdle;

        const CollisionComponent2D* col = m_scene.Collisions().TryGet(e.id);
        if (!col)
            continue;

        const bool drew = DebugDraw2D::DrawCollisionComponent(
            batch,
            t,
            *col,
            circleColor,
            circleColor,
            kColliderThickness,
            kDebugDrawLayer,
            48);

        if (drew) {
            DebugDraw2D::DrawCross(
                batch,
                t.position,
                10.0f,
                crossColor,
                kColliderThickness,
                kDebugDrawLayer);
        }
    }
}
