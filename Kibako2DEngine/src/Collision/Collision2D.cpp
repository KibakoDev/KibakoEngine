// Simple overlap tests for 2D circle and AABB colliders
#include "KibakoEngine/Collision/Collision2D.h"
#include "KibakoEngine/Scene/Scene2D.h"

namespace KibakoEngine {

    bool Intersects(const CircleCollider2D& c1, const Transform2D& t1,
                    const CircleCollider2D& c2, const Transform2D& t2)
    {
        if (!c1.active || !c2.active)
            return false;

        const float dx = t1.position.x - t2.position.x;
        const float dy = t1.position.y - t2.position.y;
        const float dist2 = (dx * dx) + (dy * dy);

        const float r = c1.radius + c2.radius;
        return dist2 <= (r * r);
    }

    bool Intersects(const AABBCollider2D& b1, const Transform2D& t1,
                    const AABBCollider2D& b2, const Transform2D& t2)
    {
        if (!b1.active || !b2.active)
            return false;

        const float ax1 = t1.position.x - b1.halfW;
        const float ax2 = t1.position.x + b1.halfW;

        const float ay1 = t1.position.y - b1.halfH;
        const float ay2 = t1.position.y + b1.halfH;

        const float bx1 = t2.position.x - b2.halfW;
        const float bx2 = t2.position.x + b2.halfW;

        const float by1 = t2.position.y - b2.halfH;
        const float by2 = t2.position.y + b2.halfH;

        return (ax1 <= bx2 && ax2 >= bx1 && ay1 <= by2 && ay2 >= by1);
    }

} // namespace KibakoEngine
