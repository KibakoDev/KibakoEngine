#pragma once

#include <cmath>
#include <random>
#include <type_traits>
#include <numbers>

namespace KibakoEngine::Math {

    inline constexpr float Pi = std::numbers::pi_v<float>;

    template <typename T>
    [[nodiscard]] constexpr T Clamp(T value, T minValue, T maxValue)
    {
        return value < minValue ? minValue : (value > maxValue ? maxValue : value);
    }

    template <typename T>
    [[nodiscard]] constexpr T Saturate(T value)
    {
        return Clamp(value, T(0), T(1));
    }

    // Consider restricting to floating point to avoid silent int truncation
    template <typename T>
    [[nodiscard]] constexpr T Lerp(T a, T b, T t)
        requires std::is_floating_point_v<T>
    {
        return a + (b - a) * t;
    }

    [[nodiscard]] inline float ToRadians(float degrees)
    {
        return degrees * (Pi / 180.0f);
    }

    [[nodiscard]] inline float ToDegrees(float radians)
    {
        return radians * (180.0f / Pi);
    }

    // Wrap for floats: [min, max)
    [[nodiscard]] inline float Wrap(float value, float minValue, float maxValue)
    {
        float range = maxValue - minValue;
        if (range == 0.0f) return minValue;
        float x = std::fmod(value - minValue, range);
        if (x < 0.0f) x += range;
        return x + minValue;
    }

    // Wrap for ints: [min, max)
    [[nodiscard]] inline int Wrap(int value, int minValue, int maxValue)
    {
        int range = maxValue - minValue;
        if (range == 0) return minValue;
        int x = (value - minValue) % range;
        if (x < 0) x += range;
        return x + minValue;
    }

    namespace Random
    {
        [[nodiscard]] inline std::mt19937& Engine()
        {
            static std::mt19937 engine(std::random_device{}());
            return engine;
        }

        [[nodiscard]] inline void Seed(uint32_t seed)
        {
            Engine().seed(seed);
        }

        [[nodiscard]] inline int Int(int min, int max) // inclusive
        {
            std::uniform_int_distribution<int> dist(min, max);
            return dist(Engine());
        }

        [[nodiscard]] inline float Float(float min, float max)
        {
            std::uniform_real_distribution<float> dist(min, max);
            return dist(Engine());
        }

        [[nodiscard]] inline float Float01()
        {
            return Float(0.0f, 1.0f);
        }

        [[nodiscard]] inline bool Bool(float trueProbability = 0.5f)
        {
            return Float01() < trueProbability;
        }

        [[nodiscard]] inline float Angle()
        {
            return Float(0.0f, 2.0f * Pi);
        }
    }

} // namespace KibakoEngine::Math