// Copyright 2023-2024 DreamWorks Animation LLC and Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cmath>


namespace arras {
namespace sdk {
namespace util {

/**
 * Converts a float into an 8-bit color value applying gamma correction and quantization
 * @param val The float value to convert
 * @return an 8-bit (0-255) value representing the gamma-corrected and quantized value
 */
inline
unsigned char
gammaAndQuantizeTo8Bit(float val)
{
    static const float zero(0.0f);
    static const float one(1.0f);
    static const float twoFiftyFive(255.0f);
    static const float gammaExponent = 1.0f / 2.2f;

    val = std::min(val, one);
    val = std::max(val, zero);
    val = std::pow(val, gammaExponent);
    val *= twoFiftyFive;
    return static_cast<unsigned char>(val);
}

/**
 * Converts a buffer of linear float image data RGBA 4 floats per pixel to 8-bit RGB(A)
 * @param data        raw RGBA float data in row-order, 4 floats per pixel
 * @param width       width of the image buffer
 * @param height      height of the image buffer
 * @param numChannels (optional) parameter to indicate the number of channels (3 or 4)
 * @return A newly allocated buffer (caller must free buffer using delete [] )
 */
inline
unsigned char*
floatToRGB888(const float* const data, unsigned int width, unsigned int height, unsigned int numChannels=3)
{
    // We have to convert from linear float to RGB(A)
    unsigned char* dest = new uchar[width * height * numChannels];
    unsigned char *tmp = dest;

    // TODO: Speed up this conversion
    for (unsigned int i = 0; i < width; i++) {
        for (unsigned int j = 0; j < height * 4; j+=4) {
            const int idx = i * (height * 4) + j;
            for (unsigned int k = 0; k < numChannels; ++k) {
                *tmp++ =gammaAndQuantizeTo8Bit(data[idx + k]);
            }
        }
    }
    return dest;
}


}
}
}
