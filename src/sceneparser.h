#pragma once

#include "scene.h"
#include "xml.h"

class Scene::SceneParser {
public:
    SceneParser(std::istream &in, const std::string &filename, scalar time) :
        m_xml(in),
        m_sceneFileName{ filename },
        m_time{ time }
    {}
    Scene parse();

private:
    struct Lights {
        Power ambientLight;
        std::vector<Light> lights;
    };
    struct TransformInfo {
        // we have them separate for now to skip calculating the inverse
        Matrix34 o2wVector = Matrix34::identity(); // object to world for vectors
        Matrix34 o2wNormal = Matrix34::identity(); // object to world for normals
        Matrix34 w2oVector = Matrix34::identity(); // world to object for vectors
    };
    struct ObjectInfo {
        Point3 position;
        Material material;
        Scene::SceneParser::TransformInfo transform;
    };
    Scene tag_scene();
    Camera tag_camera();
    Lights tag_lights();
    Light tag_light();
    std::vector<Object> tag_surfaces();
    ObjectInfo tag_object();
    Material tag_material();
    TransformInfo tag_transform();

    Color tag_color();
    Vector3 tag_vector3();

    bool tagIs(const std::string &name, Xml::TagType type) const {
        return m_xml.thisTag().is(name, type);
    }
    // converter functions
    // either with defaultValue or throwing if attribute is missing
    // attrToScalar supports animations using m_time
    const std::string &attrToString(const std::string &attrname) const;
    const std::string &attrToString(const std::string &attrname, const std::string &defaultValue) const;
    scalar attrToScalar(const std::string &attrname) const;
    scalar attrToScalar(const std::string &attrname, scalar defaultValue) const;
    u32 attrToU32(const std::string &attrname) const;
    u32 attrToU32(const std::string &attrname, u32 defaultValue) const;

    scalar animateScalar(const std::string &attrValue) const;
    static scalar ease(char easeType, scalar time);

    Xml m_xml;
    std::string m_sceneFileName;
    scalar m_time; // for animations - goes from 0.0f to 1.0f
};
