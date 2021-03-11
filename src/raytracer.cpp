#include <cmath>
#include <complex>
#include <numeric>
#include <random>
#include <thread>

#include "raytracer.h"

// TODO: refactor: remove that instance Instance and make RayTracer::raytrace static or so..
Picture RayTracer::raytrace(const Scene &scene) const {
    Picture picture(scene.camera().resolution());
    Instance instance{ *this, scene, picture };
    instance.raytrace();
    return picture;
}

RayTracer::Instance::Instance(const RayTracer &raytracer, const Scene &scene, Picture &picture) :
    m_raytracer{ raytracer },
    m_scene{ scene },
    m_picture{ picture },
    m_picSize{ picture.size() },
    m_picSizeF{ m_picSize },
    m_halfFovX{ scene.camera().fieldOfViewAngle() },
    m_halfFov{ -tanf(m_halfFovX), tanf(m_halfFovX) * m_picSizeF.aspect()},
    m_pixelSize{ -2.0f / m_picSizeF * m_halfFov },
    m_subPixelSize{ m_pixelSize * (1.0f / scene.camera().superSamplingPerAxis()) },
    m_cameraTransformation{ scene.camera().cameraTransformation() }
{
}

void RayTracer::Instance::raytrace() {
    m_NextLine.store(0, std::memory_order_relaxed);
    std::vector <std::thread> threads;
    for (u32 i = 0; i < m_scene.threads(); i++) {
        threads.emplace_back(&Thread::raytrace, Thread{ *this });
    }
    std::for_each(threads.begin(), threads.end(), [] (std::thread &t) {
        t.join();
    });
}

RayTracer::Instance::Thread::Thread(Instance &instance) :
    m_i{ instance },
    m_randGen{ std::random_device{}() }
{
}

void RayTracer::Instance::Thread::raytrace() {
    u32 y;
    while ((y = m_i.m_NextLine.fetch_add(1, std::memory_order_relaxed)) < m_i.m_picSize.y) {
        raytraceLine(y);
    }
}

void RayTracer::Instance::Thread::raytraceLine(u32 y) {
    scalar rayY = m_i.m_halfFov.y + y * m_i.m_pixelSize.y + 0.5f * m_i.m_pixelSize.y;
    const u32 initialRayCount{ m_i.m_scene.camera().superSamplingPerAxis() };

    scalar rayX = m_i.m_halfFov.x + 0.5f * m_i.m_pixelSize.x;
    for (u32 x = 0; x < m_i.m_picSize.x; x++) {        
        Radiance radiance;

        // Supersampling:
        // Cast one ray for each subpixel
        // TODO: Consider Adaptive supersampling, like described here:
        //   https://en.wikipedia.org/wiki/Supersampling#Computational_cost_and_adaptive_supersampling
        // TODO: Better supersampling patterns?
        for (u32 subY = 0; subY < initialRayCount; subY++) {
            for (u32 subX = 0; subX < initialRayCount; subX++) {
                // all this assumes camera is at origin (0, 0, 0)
                const Vector2 subDisplacement{ 
                    2.0f * (subX + 1) / (initialRayCount + 1) - 1.0f,
                    2.0f * (subY + 1) / (initialRayCount + 1) - 1.0f
                };
                // we distribute the ray targets on the area of our pixel on the image plane
                const Vector2 targetDisplacement = subDisplacement * m_i.m_pixelSize;
                const Point3 targetOnImagePlane = Point3{ rayX, rayY, -1.0f } + targetDisplacement;

                // Depth of Focus:
                // this scales the point from image plane at z = -1.0f as target
                // to the focus plane on z = -focusDistance as target along the ray (which comes from the origin)
                // -> so it is effectively just a scaling by the focusDistance
                const Point3 targetOnFocusPlane = m_i.m_cameraTransformation * (targetOnImagePlane * m_i.m_scene.camera().focusDistance());
                // we randomly distribute the ray origin on the lens area
                // TODO: maybe think about better sampling patterns for the camera lens (together with supersampling)
                const Vector2 originDisplacement{
                    (subDisplacement + Vector2{ m_randDis(m_randGen), m_randDis(m_randGen) } * (1.0f / initialRayCount)) *
                    m_i.m_scene.camera().lensSize()
                };
                 const Point3 rayOrigin = m_i.m_cameraTransformation * (Point3{ 0.0f, 0.0f, 0.0f }) + originDisplacement;

                // the ray goes from origin to the target point on the focus plane
                Ray ray(rayOrigin, targetOnFocusPlane - rayOrigin);

                // Dispersion support:
                // 8 rays (= 45 degree hue steps) look quite nice
                // TODO: consider using a precalculated HSV RGB map
                // TODO: make it configurable
                // TODO: find out why it is only half of the brightness
                if (m_i.m_scene.dispersionMode()) {
                    for (float h = 0.0f; h < 360.0f; h += 45.0f) {
                        radiance += castRay(ray, 0, h / 180.0f - 1.0f) * HSVtoRGB(h, 100.0f, 100.0f) / 4.0f;
                    }
                } else {
                    radiance += castRay(ray, 0, 0);
                }
            }
        }
        Radiance origRadiance = m_i.m_picture.get({ x, y });
        m_i.m_picture.set({ x, y }, origRadiance + radiance * (1.0f / (initialRayCount * initialRayCount)));
        rayX += m_i.m_pixelSize.x;
    }
}

Radiance RayTracer::Instance::Thread::castRay(const Ray &ray, u32 recursion, scalar wavelength) const {
    const Scene &scene = m_i.m_scene;

    if (recursion > scene.camera().maxBounces()) {
        return Radiance{};
    }

    Radiance rad = scene.background();
    scalar max_distance = INFINITE;

    // Go over all objects
    for (const auto &object : scene.objects()) {
        // Check if ray intersects the object (and intersection is the nearest found yet)
        if (const auto intersection = object.intersect(ray, max_distance)) {
            // Interesected -> calculate Radiance for pixel
            const Material &material = object.material();
            scalar cos_angle_ray_normal = std::clamp(ray.direction().dot(intersection->normal), -1.0f, 1.0f);

            if (cos_angle_ray_normal >= 0.0f && (material.transmittance == 0.0f || norm(material.refraction) == 0.0f)) {
                // we do not see back-faces of non transparent objects
                continue;
            }

            // ray intersects front face of object or its material is transparent
            // so we see it and it replaces background or any previously detected object (which must be more far way)
            rad = Radiance{};
            max_distance = intersection->distance;

            if (cos_angle_ray_normal < 0.0f) {
                // front-facing surface
                rad += calcPhong(ray, *intersection, material);
                rad += object.getPhoton(intersection->photonCoordinate);
            }

            // Let transmittance and reflectance values enable/disable refraction and reflection according
            //   https://moodle.univie.ac.at/mod/forum/discuss.php?d=1811598
            if ((material.transmittance != 0.0f || material.reflectance != 0.0f) && norm(material.refraction) > 0.0f) {
                const scalar kr = calcFresnel(material, cos_angle_ray_normal, wavelength);
                if (material.transmittance != 0.0f && kr < 1.0f) {
                    rad += calcRefraction(ray, *intersection, material, cos_angle_ray_normal, recursion, wavelength) * (1.0f - kr);
                }
                if (material.reflectance != 0.0f && kr > 0.0f) {
                    rad += calcReflection(ray, *intersection, material, cos_angle_ray_normal, recursion, wavelength) * kr;
                }
            }
            rad = rad.withoutAlpha();
        }
    }
    return rad;
}

Color RayTracer::Instance::Thread::calcTexturePixelColorWithAntiAliasing(const Picture &texture, Point2 textureCoord) const {
    UDim2 textureSize = texture.size();
    scalar tmp; // unused tmp for modff()
    // The texture is in repeat mode -> only use fractional part
    textureCoord.x = modff(textureCoord.x, &tmp);
    textureCoord.y = modff(textureCoord.y, &tmp);
    Point2 texturePixel = { textureCoord.x * (textureSize.x - 1), textureCoord.y * (textureSize.y - 1) };

    UPoint2 uTextureCoord1{
        std::clamp(static_cast<u32>(texturePixel.x), 0u, textureSize.x - 1),
        std::clamp(static_cast<u32>(texturePixel.y), 0u, textureSize.y - 1)
    };
    scalar textureFactor1 = (1 - modff(texturePixel.x, &tmp)) * (1 - modff(texturePixel.y, &tmp));
    UPoint2 uTextureCoord2{
        std::clamp(static_cast<u32>(std::ceil(texturePixel.x)), 0u, textureSize.x - 1),
        std::clamp(static_cast<u32>(texturePixel.y), 0u, textureSize.y - 1)
    };
    scalar textureFactor2 = modff(texturePixel.x, &tmp) * (1 - modff(texturePixel.y, &tmp));
    UPoint2 uTextureCoord3{
        std::clamp(static_cast<u32>(texturePixel.x), 0u, textureSize.x - 1),
        std::clamp(static_cast<u32>(std::ceil(texturePixel.y)), 0u, textureSize.y - 1)
    };
    scalar textureFactor3 = (1 - modff(texturePixel.x, &tmp)) * modff(texturePixel.y, &tmp);
    UPoint2 uTextureCoord4{
        std::clamp(static_cast<u32>(std::ceil(texturePixel.x)), 0u, textureSize.x - 1),
        std::clamp(static_cast<u32>(std::ceil(texturePixel.y)), 0u, textureSize.y - 1)
    };
    scalar textureFactor4 = modff(texturePixel.x, &tmp) * modff(texturePixel.y, &tmp);

    return texture.get(uTextureCoord1) * textureFactor1 +
        texture.get(uTextureCoord2) * textureFactor2 +
        texture.get(uTextureCoord3) * textureFactor3 +
        texture.get(uTextureCoord4) * textureFactor4;
}

Radiance RayTracer::Instance::Thread::calcPhong(const Ray &ray, const Intersection &intersection, const Material &material) const {
    const Scene &scene = m_i.m_scene;
    const Point3 point = intersection.point;
    const Vector3 normal = intersection.normal;
    Radiance rad;

    // get material color either from material or from texture
    const Color materialColor = material.texture.empty() ?
        material.color :
        calcTexturePixelColorWithAntiAliasing(material.texture, intersection.textureCoordinate);

    rad += scene.ambientLight() * materialColor * material.phong.ka;
    for (const Light &light : scene.lights()) {
        Ray lightRay = light.type() == Light::Type::Parallel ?
            Ray(point, light.direction() * -1.0f) :
            Ray(point, light.position() - point);
        lightRay.addOffset(normal * EPSILON); // remove shadow acne
        const scalar lightDistance = light.type() == Light::Type::Parallel ?
            INFINITE :
            (light.position() - lightRay.origin()).length();
        // check if light is visible
        if (std::none_of(scene.objects().begin(), scene.objects().end(),
            [lightRay, lightDistance] (const auto &objectForTest) {
                const std::optional<Intersection> lightIntersect = objectForTest.intersect(lightRay, lightDistance);
                return lightIntersect.has_value() && lightRay.direction().dot(lightIntersect->normal) < 0.0f; // only front faces cast shadows
            })) {
            // light is visible
            // TODO: why don't we decrease power with distance for point lights?
            //const Color lightPower = light.power() / (light.type() == Light::Type::Parallel ? 1 : 4.0f * PI * lightDistance * lightDistance);
            const Color lightPower = light.power();
            const Radiance diffuseRad = lightPower * materialColor * std::max(lightRay.direction().dot(normal), 0.0f) * material.phong.kd;
            const Vector3 lightReflectionVector = (normal * lightRay.direction().dot(normal) * 2 - lightRay.direction()).normalized();
            const Radiance specularRad = lightPower * pow(std::max(lightReflectionVector.dot(ray.direction() * -1), 0.0f), material.phong.exponent) * material.phong.ks;
            rad += diffuseRad + specularRad;
        }
    }
    return rad;
}

// following functions from https ://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel
//   extended with distinction (complex numbers)
//   and dispersion
scalar RayTracer::Instance::Thread::calcFresnel(const Material &material, scalar cos_angle_ray_normal, scalar wavelength) const {
    std::complex<scalar> etai = 1;
    std::complex<scalar> etat = material.refraction + wavelength * material.dispersion;
    if (cos_angle_ray_normal > 0.0f) {
        // inside
        std::swap(etai, etat);
    }
    // get the sinus of the incidence angle via the Pythagorean identity
    // and multiply it with the refraction indices quotient
    // to get the sinus of the angle the refracted (transmitted) ray
    const std::complex<scalar> sint = etai / etat * sqrtf(std::max(0.0f, 1 - cos_angle_ray_normal * cos_angle_ray_normal));
    // check if we do not have total internal reflection
    if (norm(sint) < 1.0f) {
        // no total internal reflection -> calculate reflection coefficient
        // get the cosinus of the refracted angle via the Pythagorean identity
        //const std::complex<scalar> cost = sqrtf(std::max(0.0f, 1.0f - sint * sint));
        const std::complex<scalar> cost = sqrt(1.0f - sint * sint);
        const scalar cos_angle_ray_normalAbs = fabsf(cos_angle_ray_normal);
        // TODO: check if Rs and Rp are swapped in this formulas? (not important for us, but out of curiousity..)
        const std::complex<scalar> Rs = (etat * cos_angle_ray_normalAbs - etai * cost) / (etat * cos_angle_ray_normalAbs + etai * cost);
        const std::complex<scalar> Rp = (etai * cos_angle_ray_normalAbs - etat * cost) / (etai * cos_angle_ray_normalAbs + etat * cost);
        return (norm(Rs) + norm(Rp)) / 2;
    }

    return 1.0f; // total internal reflection  
}

Radiance RayTracer::Instance::Thread::calcRefraction(const Ray &ray, const Intersection &intersection, const Material &material, scalar cos_angle_ray_normal, u32 recursion, scalar wavelength) const {
    const Point3 point = intersection.point;
    const Vector3 normal = intersection.normal;
    scalar cos_angle_ray_normalTurned{ cos_angle_ray_normal };
    Vector3 normalTurned{ normal };
    scalar refractionIndex = material.refraction.real() + wavelength * material.dispersion;
    bool outside = false;
    if (cos_angle_ray_normal <= 0.0f) {
        // outside
        outside = true;
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
        return castRay(refractionRay, recursion + 1, wavelength);
    }
    return {};
}

Radiance RayTracer::Instance::Thread::calcReflection(const Ray &ray, const Intersection &intersection, const Material &, scalar cos_angle_ray_normal, u32 recursion, scalar wavelength) const {
    const Point3 point = intersection.point;
    const Vector3 normal = intersection.normal;
    bool outside = false;
    if (cos_angle_ray_normal <= 0.0f) {
        outside = true;
    }

    const Vector3 reflectionVector = ray.direction() - normal * cos_angle_ray_normal * 2;
    Ray mirrorRay{ point, reflectionVector };
    mirrorRay.addOffset(normal * (outside ? EPSILON : -EPSILON));
    return castRay(mirrorRay, recursion + 1, wavelength);
}
