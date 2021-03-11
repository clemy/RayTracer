#include <algorithm>
#include <cmath>

#include "objects.h"

// TODO: bounding boxes
// TODO: fast intersection algorithm (kd tree?)

std::optional<Intersection> Sphere::intersect(const Ray &ray, scalar max_distance) const {
    // according https://www.scratchapixel.com/lessons/3d-basic-rendering/minimal-ray-tracer-rendering-simple-shapes/ray-sphere-intersection
    // this calculates the intersection by solving the
    // quadratic equation which is the result of combining
    // the parametric form of a ray and a sphere
    const Point3 objectRay_origin = m_world2Object * ray.origin();
    const Vector3 objectRay_direction = m_world2Object.mulWithoutTranslate(ray.direction()).normalized();
    scalar objectMax_distance = INFINITE;
    if (max_distance != INFINITE) {
        objectMax_distance = (objectRay_origin - m_world2Object * (ray.origin() + ray.direction() * max_distance)).length();
    }
    
    const Vector3 ray_center_vector = objectRay_origin - m_center;
    // a of the quadratic equation is always 1 for normalized ray directions
    const scalar b = ray_center_vector.dot(objectRay_direction);
    const scalar c = ray_center_vector.dot(ray_center_vector) - m_radius * m_radius;
    const scalar h = b * b - c;
    // the part under the sqrt is negative -> no real solution -> we do not intersect
    if (h < 0.0f) {
        return std::nullopt;
    }
    // if h = 0 we touch the sphere in 1 point
    // in any other case we have 2 solutions
    // check first the smaller value
    scalar distance = -b - sqrt(h); 
    if (distance > objectMax_distance) {
        return std::nullopt;
    }
    if (distance < 0) {
        // ray origin is inside or after the sphere
        distance = -b + sqrt(h);
        if (distance < 0 || distance > objectMax_distance) {
            // ray origin is after the sphere
            return std::nullopt;
        }
    }
    const Point3 objectIntersectionPoint = objectRay_origin + objectRay_direction * distance;
    const Vector3 objectNormal = (objectIntersectionPoint - m_center).normalized();
    const Point2 textureCoordinate{ 0.5f + std::atan2(objectNormal.x, objectNormal.z) / (2.0f * PI), 0.5f - std::asin(objectNormal.y) / PI };
    const scalar worldDistance = (ray.origin() - m_object2World * objectIntersectionPoint).length();
    return Intersection{ worldDistance, m_object2World * objectIntersectionPoint, (m_object2WorldNormals * objectNormal).normalized(), textureCoordinate, textureCoordinate };
}

std::optional<Intersection> Triangle::intersect(const Ray &ray, scalar max_distance) const {
    // Variable names in comments are from the descriptions in
    // Hughes - Computer Graphics 3rd Edition (variable name before ; ) and
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm (name after ; )
    const Vector3 edge1 = m_vertices[1].position - m_vertices[0].position; // e1; edge1
    const Vector3 edge2 = m_vertices[2].position - m_vertices[0].position; // e2; edge2

    const Vector3 raydir_edge2_normal = ray.direction().cross(edge2); // q; h
    const scalar approach_rate = edge1.dot(raydir_edge2_normal); // a; a
    //if (approach_rate < EPSILON) {  // with backface culling
    //if (fabs(approach_rate) < EPSILON) {  // without backface culling
    //    return std::nullopt;
    //}
    const Vector3 ray_c0_vector = ray.origin() - m_vertices[0].position; // s; s
    const scalar bary_weight1 = ray_c0_vector.dot(raydir_edge2_normal) / approach_rate; // weight1; u
    // compare with -EPSILON instead of 0.0f to allow a bit of overlapping of 2 triangles
    if (bary_weight1 < -EPSILON || bary_weight1 > 1.0f) {
        return std::nullopt;
    }
    const Vector3 rayorigin_edge1_normal = ray_c0_vector.cross(edge1); // r; q
    const scalar bary_weight2 = ray.direction().dot(rayorigin_edge1_normal) / approach_rate; // weight2; v
    // compare with -EPSILON instead of 0.0f to allow a bit of overlapping of 2 triangles
    if (bary_weight2 < -EPSILON || bary_weight1 + bary_weight2 > 1.0f) {
        return std::nullopt;
    }

    const scalar distance = edge2.dot(rayorigin_edge1_normal) / approach_rate; // dist; t
    if (distance < 0 || distance > max_distance) {
        return std::nullopt;
    }

    const scalar bary_weight0 = 1.0f - (bary_weight1 + bary_weight2); // weight0;

    const Point3 intersectionPoint = ray.origin() + ray.direction() * distance;
    const Vector3 normal = (
        m_vertices[0].normal * bary_weight0 +
        m_vertices[1].normal * bary_weight1 +
        m_vertices[2].normal * bary_weight2
    ).normalized();

    const Point2 textureCoordinate {
        m_vertices[0].textureCoordinate * bary_weight0 +
        m_vertices[1].textureCoordinate * bary_weight1 +
        m_vertices[2].textureCoordinate * bary_weight2
    };
    const Point2 photonCoordinate { bary_weight0, bary_weight1 };
    return Intersection{ distance, intersectionPoint, normal, textureCoordinate, photonCoordinate };
}

// many constants for the Julia Set raytracer...
// determined empirically...
static const u32 JULIA_INTERSECT_SEARCH_ITERATIONS = 10240;
static const scalar JULIA_INTERSECT_SEARCH_CONVERGENCE_LIMIT = 0.0001f;
static const scalar JULIA_INTERSECT_SEARCH_DIVERGENCE_LIMIT = 10000.0f;
static const u32 JULIA_INTERSECT_DISTANCE_ITERATIONS = 10000;
static const scalar JULIA_NORMALS_GRADIENT_DIFF = 0.005f;
static const u32 JULIA_NORMALS_GRADIENT_DISTANCE_ITERATIONS = 8;
static const bool JULIA_NORMALS_TURN_AGAINST_RAY = true;

// Estimator for distance to Julia set:
// This function is calculating the distance to the f(x) = 0 isosurface
// of our Julia set. It is based on the idea that with the uniformization
// theorem the julia set can be transformed to a unit sphere using a
// Boettcher map. The distance to the unit sphere can be calulcated
// using the Douady-Hubbard potential divided by the gradient of the
// function. In our transformed world this is the equivalent to the distance
// to the Julia set. The estimation can be a bit too large, but never more then
// the double of it, so we finally take the half of it to be sure to 
// have an upper bound of it. The finally simplified and optimized function is from:
// https://www.iquilezles.org/www/articles/distancefractals/distancefractals.htm
scalar Julia::estimateDistance(Quaternion z0, u32 iterations) const {
    Quaternion z = z0;
    scalar d2 = 1.0f;
    scalar m2 = z.squaredLength();
    for (u32 i = 0; i < iterations; i++) {
        d2 *= 4.0f * m2;
        z = z * z + m_c;
        m2 = z.squaredLength();
        if (m2 > 1e10f) {
            break;
        }
    }
    return sqrt(m2 / d2) * 0.5f * log(sqrt(m2));
}

// Calculate normals using gradient on the surface, according the idea from:
// Hart, J, Sandin, D & Kauffman, L, 1989. Ray tracing deterministic 3-D fractals. ACM SIGGRAPH Computer GraphicsJuly 1989, pp.289-296.
// There are improved algorithms available, which could be evaluated.
Vector3 Julia::estimateNormal(Quaternion pos, scalar diff) const {
    const std::array<Quaternion, 6> grad_q_diff = {
        Quaternion{ diff, 0.0f, 0.0f, 0.0f },
        Quaternion{ -diff, 0.0f, 0.0f, 0.0f },
        Quaternion{ 0.0f, diff, 0.0f, 0.0f },
        Quaternion{ 0.0f, -diff, 0.0f, 0.0f },
        Quaternion{ 0.0f, 0.0f, diff, 0.0f },
        Quaternion{ 0.0f, 0.0f, -diff, 0.0f }
    };
    std::array<scalar, 6> distances;
    
    std::transform(grad_q_diff.begin(), grad_q_diff.end(), std::begin(distances), [this, &pos] (const Quaternion &qdiff) {
        return estimateDistance(pos + qdiff, JULIA_NORMALS_GRADIENT_DISTANCE_ITERATIONS);
    });

    return Vector3{
        distances[0] - distances[1],
        distances[2] - distances[3],
        distances[4] - distances[5]
    }.normalized();
}

std::optional<Intersection> Julia::intersect(const Ray &ray, scalar max_distance) const {

    // transform back into object coordinates
    Point3 test_pos = (m_world2Object * ray.origin() - m_position) * (1.0f / m_scale);
    const Vector3 ray_direction = m_world2Object.mulWithoutTranslate(ray.direction()).normalized();

    // directly jump along the ray to the bounding box surface intersection 
    //   and start the distance estimator from that intersection point
    //   because 1. speed and 2. it seems the distance estimator does not work perfectly far away

    // jump along ray to bounding sphere of julia set, which is the sphere circumsribing a cube with edge length of 2 (-1..+1)
    const scalar BOUNDING_SPHERE_RADIUS = sqrtf(3);
    if (test_pos.length() > BOUNDING_SPHERE_RADIUS) {
        // code from sphere intersection
        // a of the quadratic equation is always 1 for normalized ray directions
        const scalar b = test_pos.dot(ray_direction);
        const scalar c = test_pos.dot(test_pos) - BOUNDING_SPHERE_RADIUS * BOUNDING_SPHERE_RADIUS;
        const scalar h = b * b - c;
        // the part under the sqrt is negative -> no real solution -> we do not intersect
        // or it is =0 -> we touch the sphere -> no interesection with julia set
        if (h <= 0.0f) {
            return std::nullopt;
        }
        // in any other case we have 2 solutions
        // check first the smaller value
        scalar distance = -b - sqrt(h);
        /* TODO: consider max_distance here
        if (distance > objectMax_distance) {
            return std::nullopt;
        }
        */
        if (distance < 0) {
            // ray origin is inside (can not be, was checked before) or after the sphere
            return std::nullopt;
        }
        test_pos = test_pos + ray_direction * distance;
    }

    // This is an experiment to look into the inside of the set, but the results are strange.
    // TODO: needs more evaluation and maybe attending more math courses...
    // try to cut through it on Z=0:
    //test_pos = test_pos + ray_direction * test_pos.z;

    scalar distance;
    for (u32 i = 0; i < JULIA_INTERSECT_SEARCH_ITERATIONS; i++) {
        Quaternion q{ test_pos.x, test_pos.y, test_pos.z, m_cutPlane };
        distance = estimateDistance(q, JULIA_INTERSECT_DISTANCE_ITERATIONS);
        if (i == 0 && distance < JULIA_INTERSECT_SEARCH_CONVERGENCE_LIMIT) {
            distance = 100 * JULIA_INTERSECT_SEARCH_CONVERGENCE_LIMIT;
            //return std::nullopt;
        } else if (distance < JULIA_INTERSECT_SEARCH_CONVERGENCE_LIMIT  ||
            distance > JULIA_INTERSECT_SEARCH_DIVERGENCE_LIMIT) {
            break;
        }
        test_pos = test_pos + ray_direction * distance;
    }

    if (distance >= JULIA_INTERSECT_SEARCH_CONVERGENCE_LIMIT) {
        return std::nullopt;
    }

    const Quaternion q{ test_pos.x, test_pos.y, test_pos.z, m_cutPlane };
    Vector3 normal = estimateNormal(q, JULIA_NORMALS_GRADIENT_DIFF);
    // simulate that it is 2-sided by turning the normal always against the ray
    if (JULIA_NORMALS_TURN_AGAINST_RAY && normal.dot(ray_direction) > 0.0f) {
        normal = normal * -1.0f;
    }

    // transform back into world coordinates
    const Point3 intersectionPoint = m_object2World * (test_pos * m_scale + m_position);
    const scalar intersectionDistance = (intersectionPoint - ray.origin()).length();
    // TODO: remove the EPSILON check here and test if it works
    //   as we have now solved the shadow acne problem in the shading code
    //   instead of the intersection code
    if (intersectionDistance < EPSILON || intersectionDistance > max_distance) {
        return std::nullopt;
    }

    const Point2 textureCoordinate{ 0, 0 }; // texturing not supported :(
    return Intersection{ intersectionDistance, intersectionPoint, (m_object2WorldNormals * normal).normalized(), textureCoordinate, textureCoordinate };
}

void Object::addPhoton(u32 textureSize, Point2 pos, Radiance rad) {
    if (m_photonMap.empty()) {
        m_photonMap = Picture{ { textureSize, textureSize } };
    }
    const UDim2 size = m_photonMap.size();
    UPoint2 uPhotonCoord{
        std::clamp(static_cast<u32>(pos.x * (size.x - 1)), 0u, size.x - 1),
        std::clamp(static_cast<u32>(pos.y * (size.y - 1)), 0u, size.y - 1)
    };
    m_photonMap.set(uPhotonCoord,
        m_photonMap.get(uPhotonCoord) + rad);
}

Radiance Object::getPhoton(Point2 pos) const {
    if (m_photonMap.empty()) {
        return Radiance{};
    }
    const UDim2 size = m_photonMap.size();
    UPoint2 uPhotonCoord{
        std::clamp(static_cast<u32>(pos.x * (size.x - 1)), 0u, size.x - 1),
        std::clamp(static_cast<u32>(pos.y * (size.y - 1)), 0u, size.y - 1)
    };
    return m_photonMap.get(uPhotonCoord);
}
