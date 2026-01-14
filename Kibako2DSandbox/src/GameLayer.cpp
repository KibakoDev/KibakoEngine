#include "GameLayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/Editor/EditorContext.h"
#include "KibakoEngine/Renderer/DebugDraw2D.h"

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "Game";
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

    const bool ok = m_scene.LoadFromFile("assets/scenes/test.scene.json", m_app.Assets());
    if (!ok) {
        KbkError(kLogChannel, "Failed to load scene: assets/scenes/test.scene.json");
        return;
    }

    // Editor: link active scene (Hierarchy/Inspector will read this)
    m_app.Editor().SetActiveScene(&m_scene);
    m_app.Editor().Select(0);

    // Resolve by NameComponent (Unity-style)
    ResolveEntityIDsFromNames();

    KbkLog(kLogChannel, "Scene loaded (%zu entities)", m_scene.Entities().size());
}

void GameLayer::OnDetach()
{
    KBK_PROFILE_SCOPE("GameLayerDetach");

    m_app.Editor().SetActiveScene(nullptr);
    m_app.Editor().Select(0);

    m_scene.Clear();

    m_entityLeft = 0;
    m_entityRight = 0;

    m_showCollisionDebug = false;
    m_lastCollision = false;
    m_time = 0.0f;
}

void GameLayer::OnUpdate(float dt)
{
    KBK_PROFILE_SCOPE("GameLayerUpdate");

    auto& input = m_app.InputSys();

    if (input.KeyPressed(SDL_SCANCODE_F1))
        m_showCollisionDebug = !m_showCollisionDebug;

    if (input.KeyPressed(SDL_SCANCODE_ESCAPE)) {
        SDL_Event quit{};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
    }

    UpdateScene(dt);
}

void GameLayer::OnRender(SpriteBatch2D& batch)
{
    KBK_PROFILE_SCOPE("GameLayerRender");

    m_scene.Render(batch);

    if (m_showCollisionDebug)
        RenderCollisionDebug(batch);
}

void GameLayer::ResolveEntityIDsFromNames()
{
    m_entityLeft = 0;
    m_entityRight = 0;

    if (auto* e = m_scene.FindByName("LeftStar"))
        m_entityLeft = e->id;
    else
        KbkWarn(kLogChannel, "Entity 'LeftStar' not found (name component missing or wrong)");

    if (auto* e = m_scene.FindByName("RightStar"))
        m_entityRight = e->id;
    else
        KbkWarn(kLogChannel, "Entity 'RightStar' not found (name component missing or wrong)");

    // Fallback safety (keeps sandbox running even if names change)
    if (m_entityLeft == 0)  m_entityLeft = 1;
    if (m_entityRight == 0) m_entityRight = 2;

    KbkLog(kLogChannel, "Resolved IDs: Left=%u Right=%u", m_entityLeft, m_entityRight);
}

void GameLayer::UpdateScene(float dt)
{
    m_time += dt;

    Entity2D* left = m_scene.FindEntity(m_entityLeft);
    Entity2D* right = m_scene.FindEntity(m_entityRight);

    // Simple demo motion
    if (left) {
        left->transform.position = { 430.0f, 450.0f };
        left->transform.rotation = m_time * 0.8f;
    }

    if (right) {
        right->transform.position = { 530.0f, 450.0f };
        right->transform.rotation = -m_time * 0.2f;
    }

    // Collision stored in component store
    bool hit = false;
    if (left && right) {
        auto* cLeft = m_scene.Collisions().TryGet(m_entityLeft);
        auto* cRight = m_scene.Collisions().TryGet(m_entityRight);

        if (cLeft && cRight && cLeft->circle && cRight->circle) {
            hit = Intersects(*cLeft->circle, left->transform,
                *cRight->circle, right->transform);
        }
    }

    m_lastCollision = hit;
    m_scene.Update(dt);
}

void GameLayer::RenderCollisionDebug(SpriteBatch2D& batch)
{
    const Color4 circleHit = Color4::White();
    const Color4 circleIdle = Color4{ 0.7f, 0.7f, 0.7f, 1.0f };
    const Color4 crossColor = Color4::White();

    const auto& collisions = m_scene.Collisions();

    for (const Entity2D& e : m_scene.Entities()) {
        if (!e.active)
            continue;

        const CollisionComponent2D* col = collisions.TryGet(e.id);
        if (!col)
            continue;

        const Color4 circleColor = m_lastCollision ? circleHit : circleIdle;

        const bool drew = DebugDraw2D::DrawCollisionComponent(
            batch,
            e.transform,
            *col,
            circleColor,
            circleColor,
            kColliderThickness,
            kDebugDrawLayer,
            48);

        if (drew) {
            DebugDraw2D::DrawCross(
                batch,
                e.transform.position,
                10.0f,
                crossColor,
                kColliderThickness,
                kDebugDrawLayer);
        }
    }
}