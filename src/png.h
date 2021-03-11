#pragma once
#include <istream>
#include <ostream>
#include <vector>

#include "types.h"

enum class PNGFilterType {
    None = 0,
    Sub = 1
};

void writePNG(std::ostream &out, const Picture &pic, scalar gain = 1.0f, PNGFilterType filterType = PNGFilterType::Sub);

void writeAPNGStart(std::ostream &out, UDim2 size, u32 frameCount);
void writeAPNGFrame(std::ostream &out, const Picture &picture, u32 frameNum, scalar fps, scalar gain = 1.0f, PNGFilterType filterType = PNGFilterType::Sub);
void writeAPNGEnd(std::ostream &out);

Picture readPNG(std::istream &inStream);
