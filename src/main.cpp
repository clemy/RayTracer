#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "photonmap.h"
#include "png.h"
#include "raytracer.h"
#include "wavefobj.h"

// TODO: refactor motion blur code into own function and combine those 4 methods into 1

// used for no frame count or frame count == 1
void renderImage(const Scene &origScene) {
    scalar startTime = origScene.time() == INFINITE ? 0.0f : origScene.time();
    Scene scene = Scene::load(origScene.sceneFileName(), startTime);
    RayTracer raytracer;
    if (scene.photonMapScanSteps() > 0.0f) {
        std::cout << "Generating photon map for caustics.. This will take some time.." << std::endl;
        PhotonMapper::generate(scene);
    }
    std::cout << "Rendering image.." << std::endl;
    auto beginTime{ std::chrono::high_resolution_clock::now() };
    const Picture picture = raytracer.raytrace(scene);
    {
        std::cout << "Writing image to " << origScene.outFileName() << std::endl;
        std::ofstream outfile(origScene.outFileName(), std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("output file could not be opened");
        }
        writePNG(outfile, picture, 1.0f);
    }
    auto endTime{ std::chrono::high_resolution_clock::now() };
    std::chrono::duration<double> runtime{ endTime - beginTime };
    std::cout << "\nFinished in " << runtime.count() << " s\n";
}

// used for no frame count or frame count == 1 and motion blur (subFrame count > 1)
void renderImageMotionBlur(const Scene &origScene) {
    scalar startTime = origScene.time() == INFINITE ? 0.0f : origScene.time();
    Scene sceneForSubFrameCount = Scene::load(origScene.sceneFileName(), startTime);
    RayTracer raytracer;
    auto beginTime{ std::chrono::high_resolution_clock::now() };
    u32 subFramesCount = sceneForSubFrameCount.subFrames();
    Picture picture{ origScene.camera().resolution() };
    for (u32 subFrame = 0; subFrame < subFramesCount; subFrame++) {
        std::cout << "Rendering image (subframe " << subFrame + 1 << " of " << subFramesCount << ")";
        if (subFrame > 0) {
            std::chrono::duration<double> elapsedTime{ std::chrono::high_resolution_clock::now() - beginTime };
            auto remainingTime = elapsedTime / subFrame * subFramesCount - elapsedTime;
            std::cout << " - Elapsed Time: " << std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count() << " s";
            std::cout << " - Remaining Time: " << std::chrono::duration_cast<std::chrono::seconds>(remainingTime).count() << " s";
        }
        std::cout << "          \r" << std::flush;
        // one subframe at the beginning of the frameTime, one at the end (=beginning of next) frameTime
        // all others distributed evenly in between
        scalar subFrameTime = static_cast<scalar>(subFrame) / (subFramesCount - 1) / origScene.frames() + startTime;
        Scene scene = Scene::load(origScene.sceneFileName(), subFrameTime);
        if (scene.photonMapScanSteps() > 0.0f) {
            //std::cout << "Generating photon map for caustics.. This will take some time.." << std::endl;
            PhotonMapper::generate(scene);
        }
        const Picture subPicture = raytracer.raytrace(scene);
        picture.mulAdd(subPicture, 1.0f / subFramesCount);
    }
    {
        std::cout << std::endl << "Writing image to " << origScene.outFileName() << std::endl;
        std::ofstream outfile(origScene.outFileName(), std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("output file could not be opened");
        }
        writePNG(outfile, picture, 1.0f);
    }
    auto endTime{ std::chrono::high_resolution_clock::now() };
    std::chrono::duration<double> runtime{ endTime - beginTime };
    std::cout << "\nFinished in " << runtime.count() << " s\n";
}

// used for frame count > 1
void renderVideo(const Scene &origScene) {
    RayTracer raytracer;
    auto beginTime{ std::chrono::high_resolution_clock::now() };
    {
        std::cout << "Writing animation to " << origScene.outFileName() << std::endl;
        std::ofstream outfile(origScene.outFileName(), std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("output file could not be opened");
        }
        writeAPNGStart(outfile, origScene.camera().resolution(), origScene.frames());
        for (u32 frame = 0; frame < origScene.frames(); frame++) {
            std::cout << "Rendering frame " << frame + 1 << " of " << origScene.frames();
            if (frame > 0) {
                std::chrono::duration<double> elapsedTime{ std::chrono::high_resolution_clock::now() - beginTime };
                auto remainingTime = elapsedTime / frame * (origScene.frames() - frame);
                std::cout << " - Elapsed Time: " << std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count() << " s";
                std::cout << " - Remaining Time: " << std::chrono::duration_cast<std::chrono::seconds>(remainingTime).count() << " s";
            }
            std::cout << "          \r" << std::flush;
            Scene scene = Scene::load(origScene.sceneFileName(), static_cast<scalar>(frame) / (origScene.frames() - 1));
            if (scene.photonMapScanSteps() > 0.0f) {
                //std::cout << "Generating photon map for caustics.. This will take some time.." << std::endl;
                PhotonMapper::generate(scene);
            }
            const Picture picture = raytracer.raytrace(scene);
            writeAPNGFrame(outfile, picture, frame, scene.fps());
        }
        writeAPNGEnd(outfile);
    }
    auto endTime{ std::chrono::high_resolution_clock::now() };
    std::chrono::duration<double> runtime{ endTime - beginTime };
    std::cout << "\nFinished in " << runtime.count() << " s\n";
}

// used for frame count > 1 and motion blur (subFrame count > 1)
void renderVideoMotionBlur(const Scene &origScene) {
    RayTracer raytracer;
    auto beginTime{ std::chrono::high_resolution_clock::now() };
    {
        std::cout << "Writing animation to " << origScene.outFileName() << std::endl;
        std::ofstream outfile(origScene.outFileName(), std::ios::binary);
        if (!outfile) {
            throw std::runtime_error("output file could not be opened");
        }
        writeAPNGStart(outfile, origScene.camera().resolution(), origScene.frames());
        u32 subFramesCount = origScene.subFrames();
        for (u32 frame = 0; frame < origScene.frames(); frame++) {
            Picture picture{ origScene.camera().resolution() };
            u32 newSubFrameCount = subFramesCount; // allow the scene file to adapt the subFrameCount over the time
            for (u32 subFrame = 0; subFrame < subFramesCount; subFrame++) {
                std::cout << "Rendering frame " << frame + 1 << " of " << origScene.frames() << " (subframe " << subFrame + 1 << " of " << subFramesCount << ")";
                if (frame > 0 || subFrame > 0) {
                    std::chrono::duration<double> elapsedTime{ std::chrono::high_resolution_clock::now() - beginTime };
                    auto remainingTime = elapsedTime / (frame * subFramesCount + subFrame) * origScene.frames() * subFramesCount - elapsedTime;
                    std::cout << " - Elapsed Time: " << std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count() << " s";
                    std::cout << " - Remaining Time: " << std::chrono::duration_cast<std::chrono::seconds>(remainingTime).count() << " s";
                }
                std::cout << "          \r" << std::flush;
                // one subframe at the beginning of the frameTime, one at the end (=beginning of next) frameTime
                // all others distributed evenly in between
                scalar subFrameTime = (static_cast<scalar>(frame) + static_cast<scalar>(subFrame) / (subFramesCount - 1)) / origScene.frames();
                Scene scene = Scene::load(origScene.sceneFileName(), subFrameTime);
                if (scene.photonMapScanSteps() > 0.0f) {
                    //std::cout << "Generating photon map for caustics.. This will take some time.." << std::endl;
                    PhotonMapper::generate(scene);
                }
                const Picture subPicture = raytracer.raytrace(scene);
                picture.mulAdd(subPicture, 1.0f / subFramesCount);
                newSubFrameCount = scene.subFrames();
            }
            writeAPNGFrame(outfile, picture, frame, origScene.fps());
            subFramesCount = newSubFrameCount;
        }
        writeAPNGEnd(outfile);
    }
    auto endTime{ std::chrono::high_resolution_clock::now() };
    std::chrono::duration<double> runtime{ endTime - beginTime };
    std::cout << "\nFinished in " << runtime.count() << " s\n";
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cout << "Usage: "<< std::endl << argv[0] << " <scene.xml> [<out.png>]" << std::endl;
        return -1;
    }
    const char *sceneFilename = argv[1];
    try {
        Scene scene = Scene::load(sceneFilename);
        if (argc >= 3) {
            scene.setOutFileName(argv[2]);
        }
        // some performance warnings
        if (scene.dispersionMode()) {
            std::cout << "Rendering with dispersion effect. This will increase rendering time." << std::endl;
        }
        if (scene.camera().superSamplingPerAxis() > 1) {
            std::cout << "Rendering with supersampling. This will increase rendering time." << std::endl;
        } else if (scene.camera().lensSize() != 0.0f) {
            throw std::runtime_error("Depth of field needs supersampling.");
        }

        if (scene.subFrames() > 1) {
            std::cout << "Rendering with motion blur. This will increase rendering time." << std::endl;
        }

        if (scene.photonMapScanSteps() > 0.0f) {
            std::cout << "Rendering with caustics. This will increase rendering time." << std::endl;
        }

        if (scene.frames() > 1 && scene.time() == INFINITE) {
            if (scene.subFrames() > 1) {
                renderVideoMotionBlur(scene);
            } else {
                renderVideo(scene);
            }
        } else {
            if (scene.subFrames() > 1) {
                renderImageMotionBlur(scene);
            } else {
                renderImage(scene);
            }
        }
    } catch (const std::exception &e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
