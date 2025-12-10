// Basic data structures shared by 2D sprites
#pragma once

#include <DirectXMath.h>

namespace KibakoEngine {

    struct RectF {
        float x = 0.0f;
        float y = 0.0f;
        float w = 0.0f;
        float h = 0.0f;

        static RectF FromXYWH(float px, float py, float pw, float ph)
        {
            return RectF{ px, py, pw, ph };
        }
    };

    struct Color4 {
        float r = 1.0f;
        float g = 1.0f;
        float b = 1.0f;
        float a = 1.0f;

        static Color4 White() { return { 1.0f, 1.0f, 1.0f, 1.0f }; }
        static Color4 Black() { return { 0.0f, 0.0f, 0.0f, 1.0f }; }
        static Color4 Transparent() { return { 1.0f, 1.0f, 1.0f, 0.0f }; }
    };

    struct SpriteInstance {
        RectF  dst;
        RectF  src;
        Color4 color;
        float  rotation = 0.0f;
        int    layer = 0;
    };

} // namespace KibakoEngine

