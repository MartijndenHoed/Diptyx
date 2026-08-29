#pragma once
#include <stdlib.h>
#include <string>
#include <vector>
#include "esp_log.h"
#include "ZipFile.h"
#include "tjpgd.h"
#include "PNGdec.h"
#undef Trace

enum class ImageFormat {
    JPEG,
    PNG,
    UNKNOWN
};


class Image {

public:
    Image (std::string imagePath,std::string basePath);
    ImageFormat format = ImageFormat::UNKNOWN;
    void prepare();
    void decodeAndScale(int newWidth, int newHeight);
    void decodePNGAndScale(int targetW, int targetH);
    void decodeJPEGAndScale(int targetW, int targetH);
    void scaleImage(int newWidth, int newHeight);
    void floydSteinbergDither();
    std::vector<uint8_t> imageData;
    int imageWidth = 0;
    int imageHeight = 0;
    bool cached = false;
private:
    static unsigned int in_func(JDEC* jd, uint8_t* buf, unsigned int len);
    static int out_func(JDEC* jd, void* bitmap, JRECT* rect);
    std::string imagePath;
    std::string basePath;
};