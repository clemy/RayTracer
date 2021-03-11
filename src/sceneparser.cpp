#include <cmath>
#include <fstream>
#include <iterator>
#include <regex>
#include <stdexcept>

#include "fspolyfill.h"
#include "png.h"
#include "sceneparser.h"
#include "wavefobj.h"
#include "xml.h"

// This scene file parser is implemented as
//   a recursive descent parser with
//   a lookahead of 1 symbol (1 xml tag)
//   and no backtracking
// It does not check for multiple (overwrites previous)
// or missing (uses default values) tags
Scene Scene::SceneParser::parse() {
    try {
        if (m_xml.nextTag().is("scene", Xml::TagType::Start)) {
            return tag_scene();
        } else {
            throw std::runtime_error("scene tag expected");
        }
    } catch (const std::exception &e) {
        throw std::runtime_error("scene file parse error at tag <" + m_xml.thisTagString() + "> Error: " + e.what());
    }
}

Scene Scene::SceneParser::tag_scene() {
    Scene scene;

    scene.m_outFileName = attrToString("output_file");
    scene.m_threads = attrToU32("threads", scene.m_threads);    

    while (!m_xml.nextTag().is("scene", Xml::TagType::End)) {        
        if (tagIs("background_color", Xml::TagType::Empty)) {
            scene.m_background = tag_color();
        } else if (tagIs("animation", Xml::TagType::Empty)) {
            scene.m_fps = attrToScalar("fps");
            scene.m_frames = static_cast<u32>(ceil(attrToScalar("length") * scene.m_fps));
        } else if (tagIs("still", Xml::TagType::Empty)) {
            scene.m_time = attrToScalar("time");
        } else if (tagIs("motionblur", Xml::TagType::Empty)) {
            scene.m_subFrames = static_cast<u32>(ceil(attrToScalar("subframes")));
        } else if (tagIs("caustic", Xml::TagType::Empty)) {
            scene.m_photonMapScanSteps = attrToScalar("steps");
            scene.m_photonMapTextureSize = attrToU32("texture_size");
            scene.m_photonMapFactor = attrToScalar("factor");
        } else if (tagIs("camera", Xml::TagType::Start)) {
            scene.m_camera = tag_camera();
        } else if (tagIs("lights", Xml::TagType::Start)) {
            Lights lights = tag_lights();
            scene.m_ambientLight = lights.ambientLight;
            scene.m_lights = std::move(lights.lights);
        } else if (tagIs("surfaces", Xml::TagType::Start)) {
            scene.m_objects = tag_surfaces();
        } else {
            throw std::runtime_error("unknown tag in scene");
        }
    }
    if (std::any_of(scene.m_objects.begin(), scene.m_objects.end(), [] (const Object &o) {
        return o.material().dispersion != 0.0f;
        })) {
        scene.m_dispersionMode = true;
    }
    return scene;
}

Camera Scene::SceneParser::tag_camera() {
    Camera camera;
    while (!m_xml.nextTag().is("camera", Xml::TagType::End)) {
        if (tagIs("position", Xml::TagType::Empty)) {
            camera.setPosition(tag_vector3());
        } else if (tagIs("lookat", Xml::TagType::Empty)) {
            camera.setLookAt(tag_vector3());
        } else if (tagIs("up", Xml::TagType::Empty)) {
            camera.setUpVector(tag_vector3());
        } else if (tagIs("horizontal_fov", Xml::TagType::Empty)) {
            camera.setFieldOfViewAngle(attrToScalar("angle") / 360 * 2 * PI);
        } else if (tagIs("resolution", Xml::TagType::Empty)) {
            camera.setResolution(UDim2{ attrToU32("horizontal"), attrToU32("vertical") });
        } else if (tagIs("max_bounces", Xml::TagType::Empty)) {
            camera.setMaxBounces(static_cast<u32>(std::lroundf(attrToScalar("n")))); // use scalar to allow animations
        } else if (tagIs("supersampling", Xml::TagType::Empty)) {
            camera.setSuperSamplingPerAxis(attrToU32("subpixels_peraxis"));
        } else if (tagIs("dof", Xml::TagType::Empty)) {
            camera.setFocusPoint(tag_vector3());
            camera.setLensSize(attrToScalar("lenssize"));
        } else {
            throw std::runtime_error("unknown tag in camera");
        }
    }
    return camera;
}

Scene::SceneParser::Lights Scene::SceneParser::tag_lights() {
    Lights lights;
    while (!m_xml.nextTag().is("lights", Xml::TagType::End)) {
        if (tagIs("ambient_light", Xml::TagType::Start)) {
            lights.ambientLight = tag_light().power();
        } else if (tagIs("parallel_light", Xml::TagType::Start)) {
            lights.lights.push_back(tag_light());
        } else if (tagIs("point_light", Xml::TagType::Start)) {
            lights.lights.push_back(tag_light());
        } else {
            throw std::runtime_error("unknown tag in lights");
        }
    }
    return lights;
}

// handles <ambient_light>, <parallel_light> and <point_light>
Light Scene::SceneParser::tag_light() {
    std::string tagName = m_xml.thisTag().name;
    Point3 position;
    Power color;
    while (!m_xml.nextTag().is(tagName, Xml::TagType::End)) {
        if (tagIs("color", Xml::TagType::Empty)) {
            color = tag_color();
        } else if (tagIs("direction", Xml::TagType::Empty)) {
            position = tag_vector3(); // for parallel light
        } else if (tagIs("position", Xml::TagType::Empty)) {
            position = tag_vector3(); // for point light
        } else {
            throw std::runtime_error("unknown tag in "+ tagName);
        }
    }
    return Light(tagName == "parallel_light" ? Light::Type::Parallel : Light::Type::Point, position, color);
}

std::vector<Object> Scene::SceneParser::tag_surfaces() {
    std::vector<Object> objects;
    while (!m_xml.nextTag().is("surfaces", Xml::TagType::End)) {
        if (tagIs("sphere", Xml::TagType::Start)) {
            const scalar radius = attrToScalar("radius");
            ObjectInfo o = tag_object();
            objects.emplace_back(o.position, radius, o.material, o.transform.w2oVector, o.transform.o2wVector, o.transform.o2wNormal);
        } else if (tagIs("mesh", Xml::TagType::Start)) {
            const std::string meshFileName = replace_filename(m_sceneFileName, attrToString("name"));
            const Mesh mesh = mesh.load(meshFileName);
            ObjectInfo o = tag_object();
            const auto meshObjects = mesh.createObjects(o.material, o.transform.o2wVector, o.transform.o2wNormal);
            objects.insert(objects.end(), meshObjects.begin(), meshObjects.end());
        } else if (tagIs("julia", Xml::TagType::Start)) {
            scalar scale = attrToScalar("scale");
            Quaternion c{
                attrToScalar("cr"),
                attrToScalar("ca"),
                attrToScalar("cb"),
                attrToScalar("cc")
            };
            scalar cutplane = attrToScalar("cutplane");
            ObjectInfo o = tag_object();
            objects.emplace_back(o.position, scale, c, cutplane, o.material, o.transform.w2oVector, o.transform.o2wVector, o.transform.o2wNormal);
        } else {
            throw std::runtime_error("unknown tag in surfaces");
        }
    }
    return objects;
}

Scene::SceneParser::ObjectInfo Scene::SceneParser::tag_object() {
    std::string tagName = m_xml.thisTag().name;
    ObjectInfo objInfo;
    while (!m_xml.nextTag().is(tagName, Xml::TagType::End)) {
        if (tagIs("position", Xml::TagType::Empty)) {
            objInfo.position = tag_vector3();
        } else if (tagIs("material_solid", Xml::TagType::Start)) {
            objInfo.material = tag_material();
        } else if (tagIs("material_textured", Xml::TagType::Start)) {
            objInfo.material = tag_material();
        } else if (tagIs("transform", Xml::TagType::Start)) {
            objInfo.transform = tag_transform();
        } else {
            throw std::runtime_error("unknown tag in " + tagName);
        }
    }
    return objInfo;
}

Material Scene::SceneParser::tag_material() {
    std::string tagName = m_xml.thisTag().name;
    Material material;
    while (!m_xml.nextTag().is(tagName, Xml::TagType::End)) {
        if (tagIs("color", Xml::TagType::Empty)) {
            material.color = tag_color();
        } else if (tagIs("texture", Xml::TagType::Empty)) {
            const std::string textureFileName = replace_filename(m_sceneFileName, attrToString("name"));
            std::ifstream infile(textureFileName, std::ios::binary);
            if (!infile) {
                throw std::runtime_error("texture file \"" + textureFileName + "\" could not be opened");
            }
            material.texture = readPNG(infile);
        } else if (tagIs("phong", Xml::TagType::Empty)) {
            material.phong.ka = attrToScalar("ka");
            material.phong.kd = attrToScalar("kd");
            material.phong.ks = attrToScalar("ks");
            material.phong.exponent = attrToScalar("exponent");
        } else if (tagIs("reflectance", Xml::TagType::Empty)) {
            material.reflectance = attrToScalar("r");
        } else if (tagIs("transmittance", Xml::TagType::Empty)) {
            material.transmittance = attrToScalar("t");
        } else if (tagIs("refraction", Xml::TagType::Empty)) {
            // complex number: index of refraction + i * extinction coefficient
            material.refraction = { attrToScalar("iof"), attrToScalar("ec", 0.0f)};
            material.dispersion = attrToScalar("disp", 0.0f);
        } else {
            throw std::runtime_error("unknown tag in " + tagName);
        }
    }
    return material;
}

Scene::SceneParser::TransformInfo Scene::SceneParser::tag_transform() {
    // TODO: maybe implement the matrix inverse and get rid of doing this multiple times?
    // w2oVector = o2wVector.inverse()
    // o2wNormal = transpose(o2wVector.inverse())
    // TODO: check why the order is wrong or how it is thought to be in the XML
    TransformInfo transformInfo;
    while (!m_xml.nextTag().is("transform", Xml::TagType::End)) {
        if (tagIs("translate", Xml::TagType::Empty)) {
            transformInfo.o2wVector = transformInfo.o2wVector * Matrix34::translation(tag_vector3());
            transformInfo.w2oVector = Matrix34::translation(tag_vector3() * -1.0f) * transformInfo.w2oVector;

        } else if (tagIs("scale", Xml::TagType::Empty)) {
            transformInfo.o2wVector = transformInfo.o2wVector * Matrix34::scale(tag_vector3());
            transformInfo.w2oVector = Matrix34::scale(1.0f / tag_vector3()) * transformInfo.w2oVector;
            transformInfo.o2wNormal = transformInfo.o2wNormal * Matrix34::scale(1.0f / tag_vector3());

        } else if (tagIs("rotateX", Xml::TagType::Empty)) {
            transformInfo.o2wVector = transformInfo.o2wVector * Matrix34::rotationX(attrToScalar("theta") * 2 * PI / 360.0f);
            transformInfo.w2oVector = Matrix34::rotationX(-attrToScalar("theta") * 2 * PI / 360.0f) * transformInfo.w2oVector;
            transformInfo.o2wNormal = transformInfo.o2wNormal * Matrix34::rotationX(attrToScalar("theta") * 2 * PI / 360.0f);

        } else if (tagIs("rotateY", Xml::TagType::Empty)) {
            transformInfo.o2wVector = transformInfo.o2wVector * Matrix34::rotationY(attrToScalar("theta") * 2 * PI / 360.0f);
            transformInfo.w2oVector = Matrix34::rotationY(-attrToScalar("theta") * 2 * PI / 360.0f) * transformInfo.w2oVector;
            transformInfo.o2wNormal = transformInfo.o2wNormal * Matrix34::rotationY(attrToScalar("theta") * 2 * PI / 360.0f);

        } else if (tagIs("rotateZ", Xml::TagType::Empty)) {
            transformInfo.o2wVector = transformInfo.o2wVector * Matrix34::rotationZ(attrToScalar("theta") * 2 * PI / 360.0f);
            transformInfo.w2oVector = Matrix34::rotationZ(-attrToScalar("theta") * 2 * PI / 360.0f) * transformInfo.w2oVector;
            transformInfo.o2wNormal = transformInfo.o2wNormal * Matrix34::rotationZ(attrToScalar("theta") * 2 * PI / 360.0f);

        } else {
            throw std::runtime_error("unknown tag in transform");
        }
    }
    return transformInfo;
}


Color Scene::SceneParser::tag_color() {
    return Color{
        attrToScalar("r"),
        attrToScalar("g"),
        attrToScalar("b"),
        attrToScalar("a", 1.0f)
    };
}

Vector3 Scene::SceneParser::tag_vector3() {
    return Vector3{
        attrToScalar("x"),
        attrToScalar("y"),
        attrToScalar("z")
    };
}

const std::string &Scene::SceneParser::attrToString(const std::string &attrname) const {
    return m_xml.thisTag().attr(attrname);
}

const std::string &Scene::SceneParser::attrToString(const std::string &attrname, const std::string &defaultValue) const {
    auto it = m_xml.thisTag().attributes.find(attrname);
    if (it == m_xml.thisTag().attributes.end()) {
        return defaultValue;
    }
    return it->second;
}

scalar Scene::SceneParser::attrToScalar(const std::string &attrname) const {
    return animateScalar(m_xml.thisTag().attr(attrname));
}

scalar Scene::SceneParser::attrToScalar(const std::string &attrname, scalar defaultValue) const {
    auto it = m_xml.thisTag().attributes.find(attrname);
    if (it == m_xml.thisTag().attributes.end()) {
        return defaultValue;
    }
    return animateScalar(it->second);
}

u32 Scene::SceneParser::attrToU32(const std::string &attrname) const {
    return std::stoul(m_xml.thisTag().attr(attrname));
}

u32 Scene::SceneParser::attrToU32(const std::string &attrname, u32 defaultValue) const {
    auto it = m_xml.thisTag().attributes.find(attrname);
    if (it == m_xml.thisTag().attributes.end()) {
        return defaultValue;
    }
    return std::stoul(it->second);
}

scalar Scene::SceneParser::animateScalar(const std::string &attrValue) const {
    // This splits animation strings into its components:
    // Example: "-1.0;1.0(n,0.5);2.0(o);3.0(0.9)"
    //   gets split into matches 0-3 with sub matches 0-4 (submatch 0 is always fully matched string)
    //   Match 0: Sub 1: -1.0
    //   Match 1: Sub 1: 1.0   Sub 2: n   Sub 3: 0.5
    //   Match 2: Sub 1: 2.0   Sub 2: o
    //   Match 3: Sub 1: 3.0                          Sub 4: 0.9
    // spaces between values are allowed, but the XML parser does not allow them yet
    const std::regex animationRegex(R"((?:^|;)\s*([+-]?[\d\.Ee+-]+)\s*(?:\(\s*([liob])(?:\s*,\s*(\+?[\d\.Ee+-]+))?\s*\)|\(\s*(\+?[\d\.Ee+-]+)?\s*\))?\s*)");
    //bool remainingChars = false; // for checking for unparsable content
    bool start = true;
    scalar value = 0.0f;
    scalar time = 0.0f;
    char easeType = 'l';
    for (auto it = std::sregex_iterator(attrValue.begin(), attrValue.end(), animationRegex, std::regex_constants::match_continuous);
        it != std::sregex_iterator(); ++it) {
        const std::smatch match = *it;
        // remainingChars = match.suffix().length() > 0; // for checking for unparsable content
        const scalar targetValue = std::stof(match[1]);
        // remember last ease type
        if (!match[2].str().empty()) {
            easeType = match[2].str()[0];
        }
        // gets the target time either from match 3 or 4; default is 1.0f, except for the first match, where it is 0.0f
        scalar targetTime = start ? 0.0f : 1.0f;
        if (!match[3].str().empty()) {
            targetTime = std::stof(match[3]);
        } else if (!match[4].str().empty()) {
            targetTime = std::stof(match[4]);
        }

        if (targetTime < 0.0f || targetTime > 1.0f) {
            throw std::runtime_error("invalid animation time");
        }

        // should we check all entries, to provide errors earlier during rendering?
        if (start || targetTime < m_time) {
            // we are after the target time already
            if (time > targetTime) {
                throw std::runtime_error("animation time not in increasing order");
            }
            value = targetValue;
            time = targetTime;
        } else {
            if (time > m_time) {
                // this keyframe is after the actual time, but we did not reach the previous keyframe
                // this condition can happen if start keyframe time is set > 0.0f
                // just return (start keyframe) value
                return value;
            }
            // m_time is between time and targetTime, we need to interpolate the value between
            // value and targetValue using the selected ease function
            return ease(easeType, (m_time - time) / (targetTime - time)) * (targetValue - value) + value;
        }

        start = false;
    }

    // we are after the last keyframe
    return value;
}

scalar Scene::SceneParser::ease(char easeType, scalar time) {
    switch (easeType) {
    case 'l':
        // Linear
        return time;
    case 'i':
        // Cubic Functions
        return std::pow(time, 3);
    case 'o':
        return 1 - std::pow(1 - time, 3);
    case 'b':
        return time < 0.5f ? std::pow(time * 2, 3) / 2 : 1 - std::pow((1 - time) * 2, 3) / 2;
    default:
        throw std::runtime_error("invalid ease function selected");
    }
}
