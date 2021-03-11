#pragma once

#include <array>
#include <complex>
#include <optional>
#include <variant>

#include "types.h"

struct Intersection {
    scalar distance;
    Point3 point;
    Vector3 normal;
    Point2 textureCoordinate;
    Point2 photonCoordinate;
};

struct Material {
    Color color;
    Picture texture;
    struct {
        scalar ka;
        scalar kd;
        scalar ks;
        scalar exponent;
    } phong;
    scalar reflectance;
    scalar transmittance;
    std::complex<scalar> refraction;
    scalar dispersion;
};

// TODO: remove position, center, scale from Objects and replace with matrix transformations
class Sphere {
public:
    Sphere(const Point3 &center, scalar radius,
        const Matrix34 &world2Object, const Matrix34 &object2World, const Matrix34 &object2WorldNormals) :
        m_center{ center },
        m_radius{ radius },
        m_world2Object { world2Object },
        m_object2World { object2World },
        m_object2WorldNormals { object2WorldNormals }
    {}

    const Point3 &center() const { return m_center; }
    scalar radius() const { return m_radius; }

    std::optional<Intersection> intersect(const Ray &ray, scalar max_distance) const;

private:
    Point3 m_center;
    scalar m_radius;
    Matrix34 m_world2Object;
    Matrix34 m_object2World;
    Matrix34 m_object2WorldNormals;
};

class Triangle {
public:
    struct Vertex {
        Point3 position;
        Vector3 normal;
        Point2 textureCoordinate;
    };

    Triangle(const std::array<Vertex, 3> &vertices) :
        m_vertices{ vertices }
    {}

    std::optional<Intersection> intersect(const Ray &ray, scalar max_distance) const;

private:
    std::array<Vertex, 3> m_vertices;
};

class Julia {
public:
    Julia(const Point3 &position, scalar scale, const Quaternion &c, scalar cutPlane,
        const Matrix34 &world2Object, const Matrix34 &object2World, const Matrix34 &object2WorldNormals) :
        m_position{ position },
        m_scale{ scale },
        m_c{ c },
        m_cutPlane{ cutPlane },
        m_world2Object{ world2Object },
        m_object2World{ object2World },
        m_object2WorldNormals{ object2WorldNormals }
    {}

    std::optional<Intersection> intersect(const Ray &ray, scalar max_distance) const;

private:
    scalar estimateDistance(Quaternion start, u32 iterations) const;
    Vector3 estimateNormal(Quaternion pos, scalar diff) const;

    Point3 m_position;
    scalar m_scale;
    Quaternion m_c;
    scalar m_cutPlane;
    Matrix34 m_world2Object;
    Matrix34 m_object2World;
    Matrix34 m_object2WorldNormals;
};

class Object {
public:
    Object(const Point3 &center, scalar radius, const Material &material,
        const Matrix34 &world2Object, const Matrix34 &object2World, const Matrix34 &object2WorldNormals) :
        m_material{ material },
        m_object{ std::in_place_type<Sphere>, center, radius, world2Object, object2World, object2WorldNormals }
    {}

    Object(const std::array<Triangle::Vertex, 3> &vertices, const Material &material) :
        m_material{ material },
        m_object{ std::in_place_type<Triangle>, vertices }
    {}

    Object(const Point3 &position, scalar scale, const Quaternion &c, scalar cutPlane, const Material &material,
        const Matrix34 &world2Object, const Matrix34 &object2World, const Matrix34 &object2WorldNormals) :
        m_material{ material },
        m_object{ std::in_place_type<Julia>, position, scale, c, cutPlane, world2Object, object2World, object2WorldNormals }
    {}

    const Material &material() const { return m_material; }
    std::optional<Intersection> intersect(const Ray &ray, scalar max_distance) const {
        return std::visit([&ray, max_distance] (const auto &obj) { return obj.intersect(ray, max_distance); }, m_object);
    }

    void addPhoton(u32 textureSize, Point2 pos, Radiance rad);
    Radiance getPhoton(Point2 pos) const;

private:
    Material m_material;
    Picture m_photonMap;
    std::variant<Sphere, Triangle, Julia> m_object;
};

// TODO: optionally spot_light
class Light {
public:
    enum class Type {
        Parallel,
        Point
    };
    Light(Type type, const Point3 &position, scalar intensity = 1.0f) :
        Light(type, position, { intensity, intensity, intensity })
    {}

    Light(Type type, const Point3 &position, const Power &power) :
        m_type{ type },
        m_position{ position },
        m_power{ power }
    {}

    Type type() const { return m_type; }
    const Point3 &position() const { return m_position; } // for point light
    const Vector3 &direction() const { return m_position; } // for parallel light
    const Power &power() const { return m_power; }

private:
    Type m_type;
    Point3 m_position; // stores direction for parallel light
    Power m_power;
};

class Camera {
public:
    Camera() { recalculateCamera(); }


    scalar fieldOfViewAngle() const { return m_fieldOfViewAngle; } // in rad
    Matrix34 cameraTransformation() const { return m_cameraTransformation; }
    UDim2 resolution() const { return m_resolution; }
    u32 maxBounces() const { return m_maxBounces; }
    u32 superSamplingPerAxis() const { return m_superSamplingPerAxis; }
    scalar focusDistance() const { return m_focusDistance; }
    scalar lensSize() const { return m_lensSize; }

    void setPosition(Point3 p) { m_position = p; recalculateCamera(); }
    void setLookAt(Point3 p) { m_lookAt = p; recalculateCamera(); }
    void setUpVector(Vector3 v) { m_upVector = v; recalculateCamera(); }
    void setFieldOfViewAngle(scalar fov) { m_fieldOfViewAngle = fov; } // in rad
    void setResolution(UDim2 resolution) { m_resolution = resolution; }
    void setMaxBounces(u32 n) { m_maxBounces = n; }
    void setSuperSamplingPerAxis(u32 v) { m_superSamplingPerAxis = v; }
    void setFocusPoint(Point3 p) { m_focusPoint = p; recalculateCamera(); }
    void setLensSize(scalar size) { m_lensSize = size; }

private:
    void recalculateCamera() {
        m_cameraTransformation = Matrix34::lookAt(m_position, m_lookAt, m_upVector);
        m_focusDistance = (m_focusPoint - m_position).length();
    }

    Point3 m_position{ 0.0f, 0.0f, 0.0f };
    Point3 m_lookAt{ 0.0f, 0.0f, -1.0f };
    Vector3 m_upVector{ 0.0f, 1.0f, 0.0f };
    scalar m_fieldOfViewAngle{ PI / 4 };
    UDim2 m_resolution{ 512, 512 };
    u32 m_maxBounces{ 8 };
    u32 m_superSamplingPerAxis{ 1 };
    Matrix34 m_cameraTransformation;
    Point3 m_focusPoint{ 0.0f, 0.0f, -1.0f };
    scalar m_focusDistance;
    scalar m_lensSize{ 0.0f };
};
