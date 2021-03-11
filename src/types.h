#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;

using scalar = float;
//constexpr scalar operator"" _s(long double v) {
//    return static_cast<scalar>(v);
//}

const scalar PI = 3.14159265358979323846f;
const scalar INFINITE = std::numeric_limits<scalar>::max();
const scalar EPSILON = 0.0001f;

// Names of data types as explained in
// Hughes - Computer Graphics 3rd Edition

struct UPoint2 {
    u32 x, y;
};
using UDim2 = UPoint2;

struct UColor {
    u8 r, g, b, a;

    std::array<u8, 3> rgb() const { return { r, g, b }; }
    std::array<u8, 4> rgba() const { return { r, g, b, a }; }

    UColor operator+(const UColor &rhs) const {
        return UColor{ static_cast<u8>(r + rhs.r), static_cast<u8>(g + rhs.g), static_cast<u8>(b + rhs.b), static_cast<u8>(a + rhs.a) };
    }  
    UColor operator-(const UColor &rhs) const {
        return UColor{ static_cast<u8>(r - rhs.r), static_cast<u8>(g - rhs.g), static_cast<u8>(b - rhs.b), static_cast<u8>(a - rhs.a) };
    }
};

struct Vector2 {
    scalar x, y;

    Vector2() : Vector2(0.0f, 0.0f) {}
    Vector2(scalar x, scalar y) : x{ x }, y{ y } {}
    explicit Vector2(UPoint2 p) : x{ static_cast<scalar>(p.x) }, y{ static_cast<scalar>(p.y) } {}

    Vector2 operator+(Vector2 rhs) const {
        return { x + rhs.x, y + rhs.y };
    }
    Vector2 operator*(scalar rhs) const {
        return { x * rhs, y * rhs };
    }
    // component-wise multiplication
    Vector2 operator*(Vector2 rhs) const {
        return { x * rhs.x, y * rhs.y };
    }
    scalar aspect() const { return y / x; }
};
inline Vector2 operator/(scalar lhs, Vector2 rhs) {
    return { lhs / rhs.x, lhs / rhs.y };
}
using Point2 = Vector2;
using Dim2 = Vector2;

struct Vector3 {
    scalar x, y, z;

    scalar length() const { return std::hypot(x, y, z); }
    Vector3 normalized() const {
        const scalar len = length();
        if (len == 0.0f) {
            return { 0.0f, 0.0f, 0.0f };
        }
        return { x / len, y / len, z / len };
    }
    Vector3 operator+(const Vector2 &rhs) const {
        return { x + rhs.x, y + rhs.y, z };
    }
    Vector3 operator-(const Vector2 &rhs) const {
        return { x - rhs.x, y - rhs.y, z };
    }  
    Vector3 operator+(const Vector3 &rhs) const {
        return { x + rhs.x, y + rhs.y, z + rhs.z };
    }
    Vector3 operator-(const Vector3 &rhs) const {
        return { x - rhs.x, y - rhs.y, z - rhs.z };
    }
    Vector3 operator*(const scalar &rhs) const {
        return { x * rhs, y * rhs, z * rhs };
    }
    scalar dot(const Vector3 &rhs) const {
        return x * rhs.x + y * rhs.y + z * rhs.z;
    }
    Vector3 cross(const Vector3 &rhs) const {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }
    friend Vector3 operator/(const scalar &lhs, const Vector3 &rhs) {
        return { lhs / rhs.x, lhs / rhs.y, lhs / rhs.z };
    }
};
using Point3 = Vector3;
using Dim3 = Vector3;

// this is a degenerated 4x4 matrix
// just usable for (normal) 3D transformations
// with the last row always 0, 0, 0, 1
struct Matrix34 {
    scalar m[3][4];

    static Matrix34 identity() {
        return {
             1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0
        };
    }

    static Matrix34 translation(Vector3 v) {
        return {
             1, 0, 0, v.x,
             0, 1, 0, v.y,
             0, 0, 1, v.z 
        };
    }

    // in rad
    static Matrix34 rotationX(scalar angle) {
        const scalar s = sinf(angle);
        const scalar c = cosf(angle);
        return {
             1, 0, 0, 0,
             0, c,-s, 0,
             0, s, c, 0
        };
    }

    // in rad
    static Matrix34 rotationY(scalar angle) {
        const scalar s = sinf(angle);
        const scalar c = cosf(angle);
        return {
             c, 0, s, 0,
             0, 1, 0, 0,
            -s, 0, c, 0
        };
    }

    // in rad
    static Matrix34 rotationZ(scalar angle) {
        const scalar s = sinf(angle);
        const scalar c = cosf(angle);
        return {
             c,-s, 0, 0,
             s, c, 0, 0,
             0, 0, 1, 0
        };
    }

    static Matrix34 scale(Vector3 v) {
        return {
             v.x,   0,   0, 0,
               0, v.y,   0, 0,
               0,   0, v.z, 0
        };
    }

    static Matrix34 lookAt(Point3 camera, Point3 target, Vector3 up) {
        // calculate the 3 base vectors of the camera coordinate system
        const Vector3 zAxis = (camera - target).normalized();
        const Vector3 xAxis = up.cross(zAxis).normalized();
        const Vector3 yAxis = zAxis.cross(xAxis).normalized();
        return {
             xAxis.x, yAxis.x, zAxis.x, camera.x,
             xAxis.y, yAxis.y, zAxis.y, camera.y,
             xAxis.z, yAxis.z, zAxis.z, camera.z
        };
    }

    Vector3 operator*(const Vector3 &rhs) const {
        return {
            rhs.x * m[0][0] + rhs.y * m[0][1] + rhs.z * m[0][2] + m[0][3],
            rhs.x * m[1][0] + rhs.y * m[1][1] + rhs.z * m[1][2] + m[1][3],
            rhs.x * m[2][0] + rhs.y * m[2][1] + rhs.z * m[2][2] + m[2][3]
        };
    }

    Vector3 mulWithoutTranslate(const Vector3 &rhs) const {
        return {
            rhs.x * m[0][0] + rhs.y * m[0][1] + rhs.z * m[0][2],
            rhs.x * m[1][0] + rhs.y * m[1][1] + rhs.z * m[1][2],
            rhs.x * m[2][0] + rhs.y * m[2][1] + rhs.z * m[2][2]
        };
    }

    Matrix34 operator*(const Matrix34 &rhs) const {
        Matrix34 ret;
        for (u8 r = 0; r < 3; r++) {
            for (u8 c = 0; c < 4; c++) {
                float s{ 0.0f };
                for (u8 i = 0; i < 3; i++) {
                    s += m[r][i] * rhs.m[i][c];
                }
                ret.m[r][c] = s;
            }
            // adapt for the 1 in the lower right corner
            // which is missed as i runs to 3 only
            // effectively adding the translation part of *this
            ret.m[r][3] += m[r][3];
        }
        return ret;
    }
};

struct Quaternion {
    scalar r, a, b, c;

    Quaternion operator+(const Quaternion &rhs) const {
        return {
            r + rhs.r,
            a + rhs.a,
            b + rhs.b,
            c + rhs.c
        };
    }

    Quaternion operator*(const scalar rhs) const {
        return {
            r * rhs,
            a * rhs,
            b * rhs,
            c * rhs
        };
    }

    // Hamilton product according
    // https://en.wikipedia.org/wiki/Quaternion
    Quaternion operator*(const Quaternion &rhs) const {
        return {
            r * rhs.r - a * rhs.a - b * rhs.b - c * rhs.c,
            r * rhs.a + a * rhs.r + b * rhs.c - c * rhs.b,
            r * rhs.b - a * rhs.c + b * rhs.r + c * rhs.a,
            r * rhs.c + a * rhs.b - b * rhs.a + c * rhs.r
        };
    }

    // Calculate length, but skip the sqrt step
    // used for optimized algorithm of
    // fractal distance estimator
    scalar squaredLength() const {
        return r * r + a * a + b * b + c * c;
    }
    scalar length() const {
        return sqrt(squaredLength());
    }
};

struct Color {
    scalar r, g, b, a;

    Color() : Color { 0.0f, 0.0f, 0.0f, 0.0f } {}
    Color(scalar _r, scalar _g, scalar _b) : r{ _r }, g{ _g }, b{ _b }, a{ 1.0f } {}
    Color(scalar _r, scalar _g, scalar _b, scalar _a) : r{ _r }, g{ _g }, b{ _b }, a{ _a } {}
    Color(UColor ucol) : r{ ucol.r / 255.0f }, g{ ucol.g / 255.0f }, b{ ucol.b / 255.0f }, a{ ucol.a / 255.0f } {}

    UColor scaleOut(scalar gain) const {
        // optional: gamma encoding with gamma 2.2
        //auto scale = [gain] (scalar v) { return static_cast<u8>(std::powf(std::clamp(v * gain, 0.0f, 1.0f), 1 / 2.2f) * 255.0f); };
        // optional: fast gamma encoding with gamma 2.0 (^(1/2.0) => sqrt)
        //auto scale = [gain] (scalar v) { return static_cast<u8>(std::sqrt(std::clamp(v * gain, 0.0f, 1.0f)) * 255.0f); };
        // optional: no gamma encoding
        auto scale = [gain] (scalar v) { return static_cast<u8>(std::clamp(v * gain, 0.0f, 1.0f) * 255.0f); };
        return UColor{ scale(r), scale(g), scale(b), scale(a) };
    }
    Color operator+(const Color &rhs) const {
        return { r + rhs.r, g + rhs.g, b + rhs.b, a + rhs.a };
    }
    const Color &operator+=(const Color &rhs) {
        r += rhs.r;
        g += rhs.g;
        b += rhs.b;
        a += rhs.a;
        return *this;
    }
    Color operator*(const scalar &rhs) const {
        return { r * rhs, g * rhs, b * rhs, a * rhs };
    }
    Color operator/(const scalar &rhs) const {
        return { r / rhs, g / rhs, b / rhs, a / rhs };
    }
    Color operator*(const Color &rhs) const {
        return { r * rhs.r, g * rhs.g, b * rhs.b, a * rhs.a };
    }
    Color withoutAlpha() const {
        return { r, g, b, 1.0f };
    }
};

using Radiance = Color;
using Power = Color;

// from https://www.codespeedy.com/hsv-to-rgb-in-cpp/
inline Color HSVtoRGB(float H, float S, float V) {
    if (H > 360 || H < 0 || S > 100 || S < 0 || V > 100 || V < 0) {
        throw std::runtime_error("out of range");
    }
    float s = S / 100;
    float v = V / 100;
    float C = s * v;
    float X = C * (1.0f - abs(fmod(H / 60.0f, 2.0f) - 1));
    float m = v - C;
    float r, g, b;
    if (H >= 0 && H < 60) {
        r = C, g = X, b = 0;
    } else if (H >= 60 && H < 120) {
        r = X, g = C, b = 0;
    } else if (H >= 120 && H < 180) {
        r = 0, g = C, b = X;
    } else if (H >= 180 && H < 240) {
        r = 0, g = X, b = C;
    } else if (H >= 240 && H < 300) {
        r = X, g = 0, b = C;
    } else {
        r = C, g = 0, b = X;
    }
    float R = (r + m);
    float G = (g + m);
    float B = (b + m);

    return Color{ R, G, B };
}

class Ray {
public:
    Ray(Point3 origin, Vector3 direction) :
        m_origin{ origin },
        m_direction{ direction.normalized() }
    {}

    const Point3 &origin() const { return m_origin; }
    const Vector3 &direction() const { return m_direction; }

    void addOffset(Vector3 offset) { m_origin = m_origin + offset; }

private:
    Point3 m_origin;
    Vector3 m_direction;
};

class Picture {
public:
    Picture() : m_size{ 0, 0 } {}

    Picture(UDim2 size) :
        m_size{ size },
        m_data{ static_cast<size_t>(size.x) * size.y }
    {}

    Picture(UDim2 size, Radiance background) :
        m_size{ size },
        m_data{ static_cast<size_t>(size.x) * size.y, background }
    {}

    void mulAdd(const Picture &rhs, scalar factor) {
        for (size_t i = 0; i < m_data.size(); i++) {
            m_data[i] += rhs.m_data[i] * factor;
        }
    }

    const UDim2 &size() const { return m_size; }
    const Radiance &get(const UPoint2 &pos) const { return m_data[datapos(pos)]; }
    void set(const UPoint2 &pos, const Radiance &radiance) { m_data[datapos(pos)] = radiance; }
    //const std::vector<Radiance> &data() const { return m_data; }
    bool empty() const { return m_size.x == 0 || m_size.y == 0; }

private:
    size_t datapos(const UPoint2 &pos) const { return static_cast<size_t>(pos.y) * m_size.x + pos.x; }

    UDim2 m_size;
    std::vector<Radiance> m_data;
};
