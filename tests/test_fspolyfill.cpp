#include <iostream>
#include <stdexcept>
#include <string>

#include "../src/fspolyfill.h"

static void test(const std::string &path, const std::string &replacement, const std::string &expected) {
    const std::string ret = replace_filename(path, replacement);
    if (ret != expected) {
        throw std::runtime_error("replace_filename(\"" + path + "\", \"" + replacement + "\") -> \"" +
            ret + "\" (expected: \"" + expected + "\")");
    }
}

int main() {
    try {
        test("", "image.png", "image.png");
        test("scene.xml", "image.png", "image.png");
        test("/scene.xml", "image.png", "/image.png");
        test("dir/scene.xml", "image.png", "dir/image.png");
        test("/dir/../xx\\scene.xml", "image.png", "/dir/../xx\\image.png");
        test("scene.xml", "../image.png", "../image.png");
        test("../scene.xml", "../image.png", "../../image.png");
        test("C:scene.xml", "image.png", "C:image.png");
        test("xyz/scene.xml", "/aa/image.png", "/aa/image.png");
        test("C:scene.xml", "D:image.png", "D:image.png");
        test("C:\\scene.xml", "image.png", "C:\\image.png");
    } catch (const std::exception &e) {
        std::cout << "test failed:" << std::endl << e.what() << std::endl;
        return -1;
    }
    std::cout << "All tests OK" << std::endl;
    return 0;
}
