#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "wavefobj.h"

Mesh Mesh::load(const std::string &filename) {
    std::ifstream file(filename);
    if (!file)
        throw std::runtime_error("mesh obj file \"" + filename + "\" could not be opened");
    return load(file);
}

// This parser implements only a small subset of the 
// Wavefront Obj File Specification
// It is silently ignoring everything it does not understand
// and only ensures that there are no invalid internal states.
// This means the loaded mesh could look unexpected!
// Supported:
//   * triangles (faces with 3 vertices)
//   * faces must contain normals
//   * faces can contain texture coordinates
Mesh Mesh::load(std::istream &in) {
    Mesh m;
    Point max{ 0, 0, 0 }; // for out of bounds checking of face indices
    for (std::string sline; std::getline(in, sline); ) {
        std::istringstream line(std::move(sline));
        std::string cmd;
        if (!(line >> cmd)) {
            continue;
        }
        if (cmd == "v") {
            scalar x, y, z;
            if (!(line >> x >> y >> z)) continue;
            m.m_vertices.push_back({ x, y, z });
        } else if (cmd == "vt") {
            scalar x, y;
            if (!(line >> x >> y)) continue;
            m.m_textureCoords.push_back({ x, y });
        } else if (cmd == "vn") {
            scalar x, y, z;
            if (!(line >> x >> y >> z)) continue;
            m.m_normals.push_back({ x, y, z });
        } else if (cmd == "f") {
            std::string p[3];
            if (!(line >> p[0] >> p[1] >> p[2])) continue;
            Mesh::Face face;
            bool error = false;
            std::transform(std::begin(p), std::end(p), std::begin(face.points), [&max, &error] (const auto &stext) {
                std::istringstream text(stext);
                u32 v, t = 0, n;
                char sep;
                if (!(text >> v >> sep) || sep != '/' || v == 0) {
                    error = true;
                    return Point{ 0, 0, 0 };
                }
                if (!(text >> t)) {
                    text.clear(); // no texture coordinate is ok
                }
                if (!(text >> sep >> n) || sep != '/' || n == 0) {
                    error = true;
                    return Point{ 0, 0, 0 };
                }
                max.vertex = std::max(max.vertex, v);
                max.textureCoord = std::max(max.textureCoord, t);
                max.normal = std::max(max.normal, n);
                return Point{ v, t, n };
            });
            if (!error) {
                m.m_faces.push_back(face);
            }
        }
    }
    if (max.normal > m.m_normals.size() || max.textureCoord > m.m_textureCoords.size() || max.vertex > m.m_vertices.size()) {
        m.clear();
        throw std::runtime_error("mesh obj file contains an out of bounds index on a face");
    }
    return m;
}

std::vector<Object> Mesh::createObjects(const Material &material, const Matrix34 &verticesTransform, const Matrix34 &normalsTransform) const {
    std::vector<Point3> vertices;
    std::vector<Point3> normals;
    std::transform(m_vertices.begin(), m_vertices.end(), std::back_inserter(vertices), [&verticesTransform] (const Point3 p) {
        return verticesTransform * p;
    });
    std::transform(m_normals.begin(), m_normals.end(), std::back_inserter(normals), [&normalsTransform] (const Vector3 n) {
        return (normalsTransform * n).normalized();
    });

    std::vector<Object> out;
    std::transform(m_faces.begin(), m_faces.end(), std::back_inserter(out), [this, &vertices, &normals, &material] (const Face &face) {
        std::array<Triangle::Vertex, 3> outVertices;
        std::transform(face.points.begin(), face.points.end(), outVertices.begin(), [this, &vertices, &normals, &material] (const Point &p) {
            return Triangle::Vertex{
                vertices[p.vertex - 1],
                normals[p.normal - 1],
                p.textureCoord > 0 ? m_textureCoords[p.textureCoord - 1] : Point2{}
            };
        });
        return Object(outVertices, material);
    });

    return out;
}

void Mesh::clear() {
    m_vertices.clear();
    m_textureCoords.clear();
    m_normals.clear();
    m_faces.clear();
}