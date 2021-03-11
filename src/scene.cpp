#include <fstream>
#include <stdexcept>
#include <string>

#include "scene.h"
#include "sceneparser.h"

Scene Scene::load(const std::string &filename, scalar time) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("scene file \"" + filename + "\" could not be opened");
    }
    Scene scene = SceneParser(file, filename, time).parse();
    scene.m_sceneFileName = filename;
    return scene;
}
