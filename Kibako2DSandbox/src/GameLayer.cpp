// Handles gameplay logic for the sandbox scene
#include "GameLayer.h"

#include "KibakoEngine/Core/Application.h"
#include "KibakoEngine/Core/Debug.h"
#include "KibakoEngine/Core/Log.h"
#include "KibakoEngine/Core/Profiler.h"
#include "KibakoEngine/Renderer/DebugDraw2D.h"
#include "KibakoEngine/UI/RmlUIContext.h"
#include "KibakoEngine/Utils/Math.h"

#include <DirectXMath.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_scancode.h>

using namespace KibakoEngine;

namespace
{
    constexpr const char* kLogChannel = "Sandbox";
    constexpr int         kDebugDrawLayer = 1000;
    constexpr float       kColliderThickness = 2.0f;

} // namespace

GameLayer::GameLayer(Application& app)
    : Layer("Sandbox.GameLayer")
    , m_app(app)
{
}

void GameLayer::OnAttach()
{
    KBK_PROFILE_SCOPE("GameLayerAttach");

    auto& assets = m_app.Assets();

    // Load the sprite texture used by all entities
    m_starTexture = assets.LoadTexture("star", "assets/star.png", true);
    if (!m_starTexture || !m_starTexture->IsValid()) {
        KbkError(kLogChannel, "Failed to load texture: assets/star.png");
        return;
    }

    // Configure shared sprite rectangles and a helper for entity setup
    const float texW = static_cast<float>(m_starTexture->Width());
    const float texH = static_cast<float>(m_starTexture->Height());
    const RectF spriteRect = RectF::FromXYWH(0.0f, 0.0f, texW, texH);
    const RectF uvRect = RectF::FromXYWH(0.0f, 0.0f, 1.0f, 1.0f);

    auto configureSprite = [&](Entity2D& e,
        const DirectX::XMFLOAT2& pos,
        const DirectX::XMFLOAT2& scale,
        const Color4& color,
        int layer)
        {
            e.transform.position = pos;
            e.transform.rotation = 0.0f;
            e.transform.scale = scale;

            e.sprite.texture = m_starTexture;
            e.sprite.dst = spriteRect;
            e.sprite.src = uvRect;
            e.sprite.color = color;
            e.sprite.layer = layer;
        };

    // Create the left star entity
    {
        Entity2D& e = m_scene.CreateEntity();
        m_entityLeft = e.id;

        configureSprite(e,
            { 0.0f, 0.0f },
            { 1.0f, 1.0f },
            Color4::White(),
            0);

        m_scene.AddCircleCollider(
            e,
            0.5f * texW * e.transform.scale.x,
            true
        );
    }

    // Create the right star entity
    {
        Entity2D& e = m_scene.CreateEntity();
        m_entityRight = e.id;

        configureSprite(e,
            { 0.0f, 0.0f },
            { 1.0f, 1.0f },
            Color4{ 0.55f, 0.55f, 0.55f, 1.0f },
            1);

        m_scene.AddCircleCollider(
            e,
            0.5f * texW * e.transform.scale.x,
            true
        );
    }

    KbkLog(kLogChannel,
        "GameLayer attached (%d x %d texture, %zu entities)",
        m_starTexture->Width(),
        m_starTexture->Height(),
        m_scene.Entities().size());
}

void GameLayer::OnDetach()
{
    KBK_PROFILE_SCOPE("GameLayerDetach");

    m_starTexture = nullptr;

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

    if (!m_starTexture || !m_starTexture->IsValid())
        return;

    m_scene.Render(batch);

    if (m_showCollisionDebug)
        RenderCollisionDebug(batch);
}

void GameLayer::UpdateScene(float dt)
{
    m_time += dt;

    Entity2D* left = m_scene.FindEntity(m_entityLeft);
    Entity2D* right = m_scene.FindEntity(m_entityRight);

    // Left star motion
    if (left) {
        left->transform.position = { 430.0f, 450.0f};
        left->transform.rotation = m_time * 0.8f;
    }

    // Right star motion
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

    // Tint sprites based on collision state
    if (left) {
        left->sprite.color = hit
            ? Color4::White()
            : Color4{ 0.9f, 0.9f, 0.9f, 1.0f };
    }

    if (right) {
        right->sprite.color = hit
            ? Color4{ 0.85f, 0.85f, 0.85f, 1.0f }
        : Color4{ 0.55f, 0.55f, 0.55f, 1.0f };
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

        const bool drew = DebugDraw2D::DrawCollisionComponent(batch,
            t,
            e.collision,
            circleColor,
            circleColor,
            kColliderThickness,
            kDebugDrawLayer,
            48);

        if (drew) {
            DebugDraw2D::DrawCross(batch,
                t.position,
                10.0f,
                crossColor,
                kColliderThickness,
                kDebugDrawLayer);
        }
    }
}
