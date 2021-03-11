#pragma once
#include <atomic>
#include <random>

#include "scene.h"

class RayTracer {
public:
    Picture raytrace(const Scene &scene) const;

private:
    class Instance;
};

class RayTracer::Instance {
public:
    Instance(const RayTracer &raytracer, const Scene &scene, Picture &picture);
    void raytrace();

private:
    class Thread;

    const RayTracer &m_raytracer;
    const Scene &m_scene;
    Picture &m_picture;
    const UDim2 m_picSize;
    const Dim2 m_picSizeF;
    const scalar m_halfFovX;
    const Point2 m_halfFov;
    const Dim2 m_pixelSize;
    const Dim2 m_subPixelSize;
    const Matrix34 m_cameraTransformation;
    std::atomic<u32> m_NextLine;
};

class RayTracer::Instance::Thread {
public:
    Thread(Instance &instance);
    void raytrace();

private:
    void raytraceLine(u32 y);
    Radiance castRay(const Ray &ray, u32 recursion, scalar wavelength) const;
    Color calcTexturePixelColorWithAntiAliasing(const Picture &texture, Point2 textureCoord) const;
    Radiance calcPhong(const Ray &ray, const Intersection &intersection, const Material &material) const;
    scalar calcFresnel(const Material &material, scalar cos_angle_ray_normal, scalar wavelength) const;
    Radiance calcRefraction(const Ray &ray, const Intersection &intersection, const Material &material, scalar cos_angle_ray_normal, u32 recursion, scalar wavelength) const;
    Radiance calcReflection(const Ray &ray, const Intersection &intersection, const Material &material, scalar cos_angle_ray_normal, u32 recursion, scalar wavelength) const;
    
    Instance &m_i;
    std::minstd_rand m_randGen;
    std::uniform_real_distribution<float> m_randDis{ -1.0f, 1.0f };
};
