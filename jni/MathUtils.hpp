#pragma once
#include <cmath>
#include "imgui.h"

struct Vector2 {
    float x, y;

    Vector2 operator-(const Vector2& v) const { return { x - v.x, y - v.y }; }
    Vector2 operator+(const Vector2& v) const { return { x + v.x, y + v.y }; }
    Vector2 operator*(float f) const { return { x * f, y * f }; }
    Vector2 operator/(float f) const { return { x / f, y / f }; }

    float length() const { return std::sqrt(x * x + y * y); }
    float dot(const Vector2& v) const { return x * v.x + y * v.y; }
    
    Vector2 normalized() const {
        float l = length();
        if (l < 0.0001f) return { 0, 0 };
        return *this / l;
    }

    // Reflect vector off a normal (for bank shots)
    Vector2 reflect(const Vector2& normal) const {
        return *this - (normal * 2.0f * this->dot(normal));
    }

    static Vector2 fromImVec2(const ImVec2& v) { return { v.x, v.y }; }
    ImVec2 toImVec2() const { return { x, y }; }
};
