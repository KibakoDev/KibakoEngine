// Handles gameplay logic for the sandbox scene
#include "GameLayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/Renderer/DebugDraw2D.h"
#include "KibakoEngine/Scene/Scene2D.h"

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

    // These IDs are defined in the JSON
    m_entityLeft = 1;
    m_entityRight = 2;

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

void GameLayer::UpdateScene(float dt)
{
    m_time += dt;

    Entity2D* left = m_scene.FindEntity(m_entityLeft);
    Entity2D* right = m_scene.FindEntity(m_entityRight);

    if (left) {
        left->transform.position = { 430.0f, 450.0f };
        left->transform.rotation = m_time * 0.8f;
    }

    if (right) {
        right->transform.position = { 530.0f, 450.0f };
        right->transform.rotation = -m_time * 0.2f;
    }

    bool hit = false;
    if (left && right &&
        left->collision.circle && right->collision.circle) {

        hit = Intersects(*left->collision.circle, left->transform,
            *right->collision.circle, right->transform);
    }

    m_lastCollision = hit;
    m_scene.Update(dt);
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

        const bool drew = DebugDraw2D::DrawCollisionComponent(
            batch,
            t,
            e.collision,
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