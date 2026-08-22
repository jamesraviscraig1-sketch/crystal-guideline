#pragma once
#include <cmath>
#include <algorithm>

struct Vector2 {
    float x = 0.f, y = 0.f;

    Vector2() = default;
    Vector2(float x, float y) : x(x), y(y) {}

    Vector2 operator+(const Vector2& v) const { return { x + v.x, y + v.y }; }
    Vector2 operator-(const Vector2& v) const { return { x - v.x, y - v.y }; }
    Vector2 operator*(float f)          const { return { x * f,   y * f   }; }
    Vector2 operator/(float f)          const { return { x / f,   y / f   }; }
    Vector2& operator+=(const Vector2& v) { x += v.x; y += v.y; return *this; }

    float length()  const { return std::sqrt(x*x + y*y); }
    float dot(const Vector2& v) const { return x*v.x + y*v.y; }

    Vector2 normalized() const {
        float l = length();
        return (l > 0.0001f) ? *this / l : Vector2{0,0};
    }

    Vector2 reflect(const Vector2& n) const {
        return *this - (n * (2.f * dot(n)));
    }

    float angle() const { return std::atan2(y, x); }

    static Vector2 fromAngle(float rad) { return { std::cos(rad), std::sin(rad) }; }
};

struct Vector3 {
    float x = 0.f, y = 0.f, z = 0.f;
    Vector2 xy() const { return { x, y }; }
};

// Linear interpolation
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vector2 Lerp(Vector2 a, Vector2 b, float t) {
    return { Lerp(a.x, b.x, t), Lerp(a.y, b.y, t) };
}

// Clamp
template<typename T>
inline T Clamp(T v, T lo, T hi) { return std::max(lo, std::min(hi, v)); }
