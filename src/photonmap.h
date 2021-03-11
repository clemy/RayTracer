#pragma once

#include "scene.h"

class PhotonMapper {
public:
    static void generate(Scene &scene);

private:
    PhotonMapper(Scene &scene) : m_scene{ scene } {}

    void generate();
    void castRay(const Ray &ray, u32 recursion, scalar wavelength, Radiance rad);

    Scene &m_scene;
};
