#include <algorithm>
#include <cmath>

#include "photonmap.h"

// This is a prototype implementation of a photon mapper for generating caustic effects.
//   This algorithm runs before the normal raytracing algorithm.
//   It casts sample rays from every light source through all reflective and transparent objects
//   until they hit a diffuse object. There the radiance is stored in a photon texture of the object
//   which is then added during raytracing to add the caustic effects.
// The castRay code is a modified copy from the code in raytracer.cpp.
//   After finalizing the idea it should be refactored and merged again with castRay from raytracer.cpp.
// As this is a copy of raytracer.cpp it also contains code parts from
//   https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel

void PhotonMapper::generate(Scene &scene) {
    PhotonMapper(scene).generate();
}

void PhotonMapper::generate() {
    for (const auto &light : m_scene.lights()) {
        // we only need to cast into the direction of objects,
        // but for that we need a method to project objects
        // (or their bounding boxes) onto our "viewsphere".
        //for (const auto &object : m_scene.objects()) {
        //    if (norm(object.material().refraction) == 0.0f) {
        //        // we only handle reflective and transparent objects
        //        continue;
        //    }            
        //}

        if (light.type() == Light::Type::Parallel) {
            // parallel lights need to cast directly on objects
            // implement later with the feature mentioned above.
            // skip them for now
            continue;
        }
        const scalar SCAN_STEP_ANGLE = 2 * PI / m_scene.photonMapScanSteps();
        for (scalar phi = 0.0f; phi < 2 * PI; phi += SCAN_STEP_ANGLE) {
            for (scalar theta = 0.0f; theta < PI; theta += SCAN_STEP_ANGLE) {
                Vector3 scanDirection{
                    sinf(theta) * cosf(phi),
                    sinf(theta) * sinf(phi),
                    cosf(theta)
                };
                Ray lightRay{ light.position(), scanDirection };

                if (m_scene.dispersionMode()) {
                    for (float h = 0.0f; h < 360.0f; h += 45.0f) {
                        castRay(lightRay, 0, h / 180.0f - 1.0f, HSVtoRGB(h, 100.0f, 100.0f) * m_scene.photonMapFactor() / 4);
                    }
                } else {
                    // TODO: light color must be considered
                    castRay(lightRay, 0, 0, Radiance{ 1.0f, 1.0f, 1.0f } * m_scene.photonMapFactor());
                }
            }
        }
    }
}

void PhotonMapper::castRay(const Ray &ray, u32 recursion, scalar wavelength, Radiance rad) {
    if (recursion > m_scene.camera().maxBounces()) {
        return;
    }
    scalar max_distance = INFINITE;
    Object *nearestObject = nullptr;
    Intersection nearestIntersection;
    for (Object &object : m_scene.objects()) {
        if (const auto intersection = object.intersect(ray, max_distance)) {
            max_distance = intersection->distance;
            nearestObject = &object;
            nearestIntersection = intersection.value();
        }
    }
    if (nearestObject == nullptr) {
        // no object intersected -> return
        return;
    }
    if (norm(nearestObject->material().refraction) <= 0.0f) {
        // the lightray ends here, store it
        if (recursion > 0) {
            nearestObject->addPhoton(m_scene.photonMapTextureSize(), nearestIntersection.photonCoordinate, rad);
        }

    } else {
        // calculate refraction
        const Material &material = nearestObject->material();
        const Point3 point = nearestIntersection.point;
        const Vector3 normal = nearestIntersection.normal;
        scalar cos_angle_ray_normal = std::clamp(ray.direction().dot(normal), -1.0f, 1.0f);

        std::complex<scalar> etai = 1;
        std::complex<scalar> etat = material.refraction + wavelength * material.dispersion;
        bool outside = true;
        if (cos_angle_ray_normal > 0.0f) {
            // inside
            outside = false;
            std::swap(etai, etat);
        }
        // get the sinus of the incidence angle via the Pythagorean identity
        // and multiply it with the refraction indices quotient
        // to get the sinus of the angle the refracted (transmitted) ray
        const std::complex<scalar> sint = etai / etat * sqrtf(std::max(0.0f, 1 - cos_angle_ray_normal * cos_angle_ray_normal));
        scalar kr;
        // check if we do not have total internal reflection
        if (norm(sint) < 1.0f) {
            // no total internal reflection -> calculate reflection coefficient
            // get the cosinus of the refracted angle via the Pythagorean identity
            const std::complex<scalar> cost = sqrt(1.0f - sint * sint);
            const scalar cos_angle_ray_normalAbs = fabsf(cos_angle_ray_normal);
            const std::complex<scalar> Rs = (etat * cos_angle_ray_normalAbs - etai * cost) / (etat * cos_angle_ray_normalAbs + etai * cost);
            const std::complex<scalar> Rp = (etai * cos_angle_ray_normalAbs - etat * cost) / (etai * cos_angle_ray_normalAbs + etat * cost);
            kr = (norm(Rs) + norm(Rp)) / 2;
        } else {
            kr = 1.0f; // total internal reflection  
        }

        if (kr < 1.0f) {
            // refraction according
            // https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel
            scalar cos_angle_ray_normalTurned{ cos_angle_ray_normal };
            Vector3 normalTurned{ normal };
            scalar refractionIndex = material.refraction.real() + wavelength * material.dispersion;
            if (outside) {
                // outside
                cos_angle_ray_normalTurned *= -1.0f;
                refractionIndex = 1.0f / refractionIndex;
            } else {
                // inside
                normalTurned = normalTurned * -1.0f;
            }
            const scalar k = 1.0f - refractionIndex * refractionIndex * (1.0f - cos_angle_ray_normalTurned * cos_angle_ray_normalTurned);
            if (k >= 0) {
                const Vector3 refractionVector = ray.direction() * refractionIndex + normalTurned * (refractionIndex * cos_angle_ray_normalTurned - sqrtf(k));
                Ray refractionRay{ point, refractionVector };
                refractionRay.addOffset(normal * (outside ? -EPSILON : EPSILON));
                castRay(refractionRay, recursion + 1, wavelength, rad * (1 - kr));
            }
        }

        const Vector3 reflectionVector = ray.direction() - normal * cos_angle_ray_normal * 2;
        Ray mirrorRay{ point, reflectionVector };
        mirrorRay.addOffset(normal * (outside ? EPSILON : -EPSILON));
        castRay(mirrorRay, recursion + 1, wavelength, rad * kr);
    }
}
