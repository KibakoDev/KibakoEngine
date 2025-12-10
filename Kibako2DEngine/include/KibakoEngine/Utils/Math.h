// Lightweight math helpers and random number utilities
#pragma once

#include <cmath>
#include <random>
#include <type_traits>

namespace KibakoEngine::Math {

    template <typename T>
    [[nodiscard]] constexpr T Clamp(const T& value, const T& minValue, const T& maxValue)
    {
        return value < minValue ? minValue : (value > maxValue ? maxValue : value);
    }

    template <typename T>
    [[nodiscard]] constexpr T Lerp(const T& a, const T& b, float t)
    {
        return static_cast<T>(a + (b - a) * t);
    }

    template <typename T>
    [[nodiscard]] constexpr T Saturate(const T& value)
    {
        return Clamp(value, T(0), T(1));
    }

    [[nodiscard]] inline float ToRadians(float degrees)
    {
        return degrees * (3.1415926535f / 180.0f);
    }

    [[nodiscard]] inline float ToDegrees(float radians)
    {
        return radians * (180.0f / 3.1415926535f);
    }

    template <typename T>
    [[nodiscard]] inline T Wrap(T value, T minValue, T maxValue)
    {
        const T range = maxValue - minValue;
        if (range == T(0))
            return minValue;
        while (value < minValue)
            value += range;
        while (value >= maxValue)
            value -= range;
        return value;
    }

    namespace Random
    {
        [[nodiscard]] inline std::mt19937& Engine()
        {
            static std::random_device rd;
            static std::mt19937 engine(rd());
            return engine;
        }

        [[nodiscard]] inline int Int(int min, int max)
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
            return Float(0.0f, 6.28318530718f);
        }
    } // namespace Random

} // namespace KibakoEngine::Math
