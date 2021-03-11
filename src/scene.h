#pragma once

#include <istream>
#include <memory>
#include <string>
#include <vector>

#include "objects.h"

class Scene {
public:
    static Scene load(const std::string &filename, scalar time = 0.0f);

    const std::string &sceneFileName() const { return m_sceneFileName; }
    const std::string &outFileName() const { return m_outFileName; }
    u32 threads() const { return m_threads; }
    scalar time() const { return m_time; }
    u32 frames() const { return m_frames; }
    scalar fps() const { return m_fps; }
    u32 subFrames() const { return m_subFrames; }
    const Camera &camera() const { return m_camera; }
    Radiance background() const { return m_background; }
    Power ambientLight() const { return m_ambientLight; }
    const std::vector<Light> &lights() const { return m_lights; }
    std::vector<Object> &objects() { return m_objects; }
    const std::vector<Object> &objects() const { return m_objects; }
    bool dispersionMode() const { return m_dispersionMode; }
    scalar photonMapScanSteps() const { return m_photonMapScanSteps; }
    u32 photonMapTextureSize() const { return m_photonMapTextureSize; }
    scalar photonMapFactor() const { return m_photonMapFactor; }

    void setOutFileName(const std::string &name) { m_outFileName = name; }

    class SceneParser;

private:
    std::string m_sceneFileName;
    std::string m_outFileName;
    u32 m_threads{ 8 };
    scalar m_time{ INFINITE };
    u32 m_frames{ 1 }; // frame count - for the animation extension
    scalar m_fps{ 25.0f }; // frames per second - for the animation extension
    u32 m_subFrames{ 1 }; // subframes - for the motion blur extension
    Camera m_camera;
    Radiance m_background{ 0.0f, 0.0f, 0.0f, 0.0f };
    Power m_ambientLight;
    std::vector<Light> m_lights;
    std::vector<Object> m_objects;
    bool m_dispersionMode = false;
    scalar m_photonMapScanSteps = 0.0f;
    u32 m_photonMapTextureSize = 0;
    scalar m_photonMapFactor = 0.0f;
};