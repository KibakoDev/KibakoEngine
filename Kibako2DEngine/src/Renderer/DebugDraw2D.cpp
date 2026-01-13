// Utility helpers for drawing debug shapes with SpriteBatch2D
#include "KibakoEngine/Renderer/DebugDraw2D.h"
#include "KibakoEngine/Scene/Scene2D.h"

#include <algorithm>
#include <cmath>

namespace KibakoEngine::DebugDraw2D {

    namespace
    {
        constexpr RectF kUnitRect{ 0.0f, 0.0f, 1.0f, 1.0f };

        const Texture2D* ResolveTexture(SpriteBatch2D& batch)
        {
            if (const Texture2D* texture = batch.DefaultWhiteTexture())
                return texture;
            return nullptr;
        }
    }

    void DrawLine(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& a,
        const DirectX::XMFLOAT2& b,
        const Color4& color,
        float thickness,
        int layer)
    {
        if (thickness <= 0.0f)
            return;

        const Texture2D* texture = ResolveTexture(batch);
        if (!texture || !texture->IsValid())
            return;

        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float length = std::sqrt((dx * dx) + (dy * dy));
        if (length <= 0.0001f)
            return;

        const float midX = (a.x + b.x) * 0.5f;
        const float midY = (a.y + b.y) * 0.5f;

        RectF dst{};
        dst.w = length;
        dst.h = thickness;
        dst.x = midX - (dst.w * 0.5f);
        dst.y = midY - (dst.h * 0.5f);

        const float angle = std::atan2(dy, dx);

        batch.Push(*texture, dst, kUnitRect, color, angle, layer);
    }

    void DrawCross(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float size,
        const Color4& color,
        float thickness,
        int layer)
    {
        const float half = size * 0.5f;
        const DirectX::XMFLOAT2 left{ center.x - half, center.y };
        const DirectX::XMFLOAT2 right{ center.x + half, center.y };
        const DirectX::XMFLOAT2 top{ center.x, center.y - half };
        const DirectX::XMFLOAT2 bottom{ center.x, center.y + half };

        DrawLine(batch, left, right, color, thickness, layer);
        DrawLine(batch, top, bottom, color, thickness, layer);
    }

    void DrawCircleOutline(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float radius,
        const Color4& color,
        float thickness,
        int layer,
        int segments)
    {
        if (radius <= 0.0f)
            return;

        segments = std::max(segments, 3);

        DirectX::XMFLOAT2 prev{ center.x + radius, center.y };
        const float step = DirectX::XM_2PI / static_cast<float>(segments);

        for (int i = 1; i <= segments; ++i) {
            const float angle = step * static_cast<float>(i);
            DirectX::XMFLOAT2 next{
                center.x + std::cos(angle) * radius,
                center.y + std::sin(angle) * radius
            };
            DrawLine(batch, prev, next, color, thickness, layer);
            prev = next;
        }
    }

    void DrawAABBOutline(SpriteBatch2D& batch,
        const DirectX::XMFLOAT2& center,
        float halfWidth,
        float halfHeight,
        const Color4& color,
        float thickness,
        int layer)
    {
        const DirectX::XMFLOAT2 tl{ center.x - halfWidth, center.y - halfHeight };
        const DirectX::XMFLOAT2 tr{ center.x + halfWidth, center.y - halfHeight };
        const DirectX::XMFLOAT2 br{ center.x + halfWidth, center.y + halfHeight };
        const DirectX::XMFLOAT2 bl{ center.x - halfWidth, center.y + halfHeight };

        DrawLine(batch, tl, tr, color, thickness, layer);
        DrawLine(batch, tr, br, color, thickness, layer);
        DrawLine(batch, br, bl, color, thickness, layer);
        DrawLine(batch, bl, tl, color, thickness, layer);
    }

    bool DrawCircleCollider(SpriteBatch2D& batch,
        const Transform2D& transform,
        const CircleCollider2D& collider,
        const Color4& color,
        float thickness,
        int layer,
        int segments)
    {
        if (!collider.active)
            return false;

        DrawCircleOutline(batch,
            transform.position,
            collider.radius,
            color,
            thickness,
            layer,
            segments);

        return true;
    }

    bool DrawAABBCollider(SpriteBatch2D& batch,
        const Transform2D& transform,
        const AABBCollider2D& collider,
        const Color4& color,
        float thickness,
        int layer)
    {
        if (!collider.active)
            return false;

        DrawAABBOutline(batch,
            transform.position,
            collider.halfW,
            collider.halfH,
            color,
            thickness,
            layer);

        return true;
    }

    bool DrawCollisionComponent(SpriteBatch2D& batch,
        const Transform2D& transform,
        const CollisionComponent2D& component,
        const Color4& circleColor,
        const Color4& aabbColor,
        float thickness,
        int layer,
        int circleSegments)
    {
        bool drewAny = false;

        if (component.circle)
            drewAny |= DrawCircleCollider(batch, transform, *component.circle, circleColor, thickness, layer, circleSegments);

        if (component.aabb)
            drewAny |= DrawAABBCollider(batch, transform, *component.aabb, aabbColor, thickness, layer);

        return drewAny;
    }

} // namespace KibakoEngine::DebugDraw2D

