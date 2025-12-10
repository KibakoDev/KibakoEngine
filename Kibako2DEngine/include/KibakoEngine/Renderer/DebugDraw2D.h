// Helpers for drawing simple debug shapes in 2D
#pragma once

#include <DirectXMath.h>

#include "KibakoEngine/Renderer/SpriteBatch2D.h"
#include "KibakoEngine/Renderer/SpriteTypes.h"

namespace KibakoEngine {
    struct Transform2D;
    struct CircleCollider2D;
    struct AABBCollider2D;
    struct CollisionComponent2D;
}

namespace KibakoEngine::DebugDraw2D {

    void DrawLine(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& a,
        const DirectX::XMFLOAT2& b,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0);

    void DrawCross(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float size,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0);

    void DrawCircleOutline(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float radius,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0,
        int segments = 32);

    void DrawAABBOutline(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float halfWidth,
        float halfHeight,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0);

    bool DrawCircleCollider(SpriteBatch2D& batch,
        const Transform2D& transform,
        const CircleCollider2D& collider,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0,
        int segments = 32);

    bool DrawAABBCollider(SpriteBatch2D& batch,
        const Transform2D& transform,
        const AABBCollider2D& collider,
        const Color4& color,
        float thickness = 1.0f,
        int layer = 0);

    bool DrawCollisionComponent(SpriteBatch2D& batch,
        const Transform2D& transform,
        const CollisionComponent2D& component,
        const Color4& circleColor,
        const Color4& aabbColor,
        float thickness = 1.0f,
        int layer = 0,
        int circleSegments = 32);

} // namespace KibakoEngine::DebugDraw2D

