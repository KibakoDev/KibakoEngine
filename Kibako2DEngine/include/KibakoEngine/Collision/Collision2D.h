// Basic 2D collider shapes and intersection helpers
#pragma once

namespace KibakoEngine {

    // Forward declaration to avoid pulling transform header
    struct Transform2D;

    struct CircleCollider2D
    {
        float radius = 0.0f;
        bool  active = true;
    };

    struct AABBCollider2D
    {
        float halfW = 0.0f;
        float halfH = 0.0f;
        bool  active = true;
    };

    struct CollisionComponent2D
    {
        CircleCollider2D* circle = nullptr;
        AABBCollider2D*   aabb = nullptr;
    };

    bool Intersects(const CircleCollider2D& c1, const Transform2D& t1,
                    const CircleCollider2D& c2, const Transform2D& t2);

    bool Intersects(const AABBCollider2D& b1, const Transform2D& t1,
                    const AABBCollider2D& b2, const Transform2D& t2);

} // namespace KibakoEngine
