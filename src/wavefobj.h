#pragma once

#include <istream>
#include <string>
#include <vector>

#include "objects.h"
#include "types.h"

// supports only triangles
class Mesh {
public:
    static Mesh load(const std::string &filename);
    static Mesh load(std::istream &in);

    std::vector<Object> createObjects(const Material &material, const Matrix34 &verticesTransform, const Matrix34 &normalsTransform) const;

    void clear();

private:
    struct Point {
        u32 vertex;
        u32 textureCoord;
        u32 normal;
    };
    struct Face {
        std::array<Point, 3> points;
    };

    std::vector<Point3> m_vertices;
    std::vector<Point2> m_textureCoords;
    std::vector<Point3> m_normals;
    std::vector<Mesh::Face> m_faces;
};
