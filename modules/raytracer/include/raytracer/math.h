#pragma once

#include <cmath>
#include <cstdint>

namespace rt {

struct Vec3 {
    float x = 0, y = 0, z = 0;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3& operator+=(const Vec3& o) {
        x += o.x;
        y += o.y;
        z += o.z;
        return *this;
    }
    constexpr Vec3& operator*=(float s) {
        x *= s;
        y *= s;
        z *= s;
        return *this;
    }
    constexpr float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

constexpr Vec3 operator+(const Vec3& a, const Vec3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
constexpr Vec3 operator-(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
constexpr Vec3 operator*(const Vec3& a, const Vec3& b) {
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}
constexpr Vec3 operator*(const Vec3& a, float s) {
    return {a.x * s, a.y * s, a.z * s};
}
constexpr Vec3 operator*(float s, const Vec3& a) {
    return a * s;
}
constexpr Vec3 operator/(const Vec3& a, float s) {
    return a * (1.0f / s);
}

constexpr float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
constexpr Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& a) {
    return std::sqrt(dot(a, a));
}
inline Vec3 normalize(const Vec3& a) {
    float l = length(a);
    return l > 0 ? a / l : Vec3{0, 0, 0};
}
constexpr Vec3 reflect(const Vec3& v, const Vec3& n) {
    return v - n * (2.0f * dot(v, n));
}
constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) {
    return a * (1.0f - t) + b * t;
}
constexpr float maxComponent(const Vec3& a) {
    return a.x > a.y ? (a.x > a.z ? a.x : a.z) : (a.y > a.z ? a.y : a.z);
}

struct Ray {
    Vec3 origin;
    Vec3 direction;
    Vec3 at(float t) const { return origin + direction * t; }
};

// PCG32 random number generator: tiny, fast and deterministic across platforms.
class Rng {
  public:
    Rng(uint64_t seed = 0x853c49e6748fea9bULL, uint64_t seq = 1) {
        m_state = 0;
        m_inc = (seq << 1u) | 1u;
        next();
        m_state += seed;
        next();
    }

    uint32_t next() {
        uint64_t old = m_state;
        m_state = old * 6364136223846793005ULL + m_inc;
        uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Uniform in [0, 1).
    float nextFloat() { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }

    Vec3 inUnitSphere() {
        for (;;) {
            Vec3 p{nextFloat() * 2 - 1, nextFloat() * 2 - 1, nextFloat() * 2 - 1};
            if (dot(p, p) < 1.0f)
                return p;
        }
    }
    Vec3 unitVector() { return normalize(inUnitSphere()); }
    Vec3 inUnitDisk() {
        for (;;) {
            Vec3 p{nextFloat() * 2 - 1, nextFloat() * 2 - 1, 0};
            if (dot(p, p) < 1.0f)
                return p;
        }
    }

  private:
    uint64_t m_state;
    uint64_t m_inc;
};

} // namespace rt
