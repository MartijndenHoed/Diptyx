#include "imageHandler.h"
#include "esp_heap_caps.h"  // for heap_caps_get_free_size
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>

static const char *TAG = "Image";

struct PNGContext {
    uint8_t* out;
    int outW;
    int outH;

    int srcW;
    int srcH;

    int currentY;
};

static PNGContext* s_pngCtx = nullptr;

static ImageFormat detectFormat(const std::string& path)
{
    auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return ImageFormat::UNKNOWN;

    std::string ext = path.substr(dot + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == "jpg" || ext == "jpeg") return ImageFormat::JPEG;
    if (ext == "png")                 return ImageFormat::PNG;

    return ImageFormat::UNKNOWN;
}



void pngLineCallback(PNGDRAW* pDraw)
{
    PNGContext* ctx = s_pngCtx;
    if (!ctx || !ctx->out) return;
    ESP_LOGI(TAG,
        "PNGDRAW: y=%d | width=%d | pitch=%d | pixelType=%d | bpp=%d",
        pDraw->y,
        pDraw->iWidth,
        pDraw->iPitch,
        pDraw->iPixelType,
        pDraw->iBpp
    );
    int y = pDraw->y;
    if (y >= ctx->outH) return;

    uint8_t* src = (uint8_t*)pDraw->pPixels;

    // Bytes per pixel (PNGdec provides this)
    int bpp = pDraw->iPitch / pDraw->iWidth;

    for (int x = 0; x < ctx->outW; x++) {

        int srcX = (x * ctx->srcW) / ctx->outW;
        uint8_t gray = 0;

        if (bpp == 1) {
            // Grayscale
            gray = src[srcX];
        }
        else if (bpp == 2) {
            // Grayscale + Alpha
            uint8_t g = src[srcX * 2 + 0];
            uint8_t a = src[srcX * 2 + 1];

            // Alpha blend to white background
            gray = (g * a + 255 * (255 - a)) >> 8;
        }
        else if (bpp == 3) {
            // RGB
            uint8_t r = src[srcX * 3 + 0];
            uint8_t g = src[srcX * 3 + 1];
            uint8_t b = src[srcX * 3 + 2];

            gray = (r * 299 + g * 587 + b * 114) / 1000;
        }
        else if (bpp == 4) {
            // RGBA
            uint8_t r = src[srcX * 4 + 0];
            uint8_t g = src[srcX * 4 + 1];
            uint8_t b = src[srcX * 4 + 2];
            uint8_t a = src[srcX * 4 + 3];

            // Convert to grayscale first
            uint8_t lum = (r * 299 + g * 587 + b * 114) / 1000;

            // Alpha blend to white background
            gray = (lum * a + 255 * (255 - a)) >> 8;
        }

        ctx->out[y * ctx->outW + x] = gray;
    }
}


struct MemJpeg {
    const uint8_t* data;
    size_t size;
    size_t offset;
};

struct OutCtx {
    std::vector<uint8_t>* out;  // grayscale 0–255
    int img_w;
    int img_h;
};

struct JpegContext {
    MemJpeg src;
    OutCtx out;
};

Image::Image(std::string imagePath, std::string basePath)
: imagePath(std::move(imagePath)), basePath(std::move(basePath))
{}

unsigned int Image::in_func(JDEC* jd, uint8_t* buf, unsigned int len) {
    auto* wrapper = static_cast<JpegContext*>(jd->device);
    auto* src = &wrapper->src;

    if (buf) {
        if (src->offset + len > src->size) len = src->size - src->offset;
        memcpy(buf, src->data + src->offset, len);
        src->offset += len;
        return len;
    } else {
        src->offset += len;
        if (src->offset > src->size) src->offset = src->size;
        return len;
    }
}

int Image::out_func(JDEC* jd, void* bitmap, JRECT* rect) {
    auto* wrapper = static_cast<JpegContext*>(jd->device);
    auto* ctx = &wrapper->out;
    auto* src = static_cast<uint8_t*>(bitmap);

    int bw = rect->right - rect->left + 1;
    int bh = rect->bottom - rect->top + 1;

    for (int y = 0; y < bh; y++) {
        for (int x = 0; x < bw; x++) {
            uint8_t r = src[3 * (y * bw + x) + 0];
            uint8_t g = src[3 * (y * bw + x) + 1];
            uint8_t b = src[3 * (y * bw + x) + 2];
            uint8_t gray = (r * 299 + g * 587 + b * 114) / 1000;

            int px = rect->left + x;
            int py = rect->top + y;
            (*ctx->out)[py * ctx->img_w + px] = gray;
        }
    }
    return 1; // success
}

void Image::prepare()
{
    format = detectFormat(imagePath);

    if (format == ImageFormat::UNKNOWN) {
        ESP_LOGE(TAG, "Unsupported image format: %s", imagePath.c_str());
        return;
    }

    // ---- Load minimal data ----
    char* data = nullptr;
    size_t size = 0;

    auto loadData = [&](size_t minSize) -> bool {
        if (basePath.empty()) {
            FILE* file = fopen(imagePath.c_str(), "rb");
            if (!file) return false;

            fseek(file, 0, SEEK_END);
            size = ftell(file);
            fseek(file, 0, SEEK_SET);

            if (size < minSize) {
                fclose(file);
                return false;
            }

            data = (char*)malloc(size);
            if (!data) {
                fclose(file);
                return false;
            }

            fread(data, 1, size, file);
            fclose(file);
        } else {
            ZipFile zip(basePath.c_str());
            data = (char*)zip.read_file_to_memory(imagePath.c_str(), &size);
            if (!data || size < minSize) {
                return false;
            }
        }
        return true;
    };

    // ============================
    // JPEG
    // ============================
    if (format == ImageFormat::JPEG) {
        constexpr size_t WORK_BUF_SIZE = 4096;
        uint8_t work_buf[WORK_BUF_SIZE];
        JDEC jd;

        if (!loadData(1024)) {
            ESP_LOGE(TAG, "Failed to load JPEG header");
            return;
        }

        JpegContext ctx {
            { reinterpret_cast<const uint8_t*>(data), size, 0 },
            { nullptr, 0, 0 }
        };

        JRESULT res = jd_prepare(&jd, in_func, work_buf, WORK_BUF_SIZE, &ctx);
        free(data);

        if (res != JDR_OK) {
            ESP_LOGE(TAG, "JPEG prepare failed: %d", res);
            return;
        }

        imageWidth  = jd.width;
        imageHeight = jd.height;

        ESP_LOGI(TAG, "JPEG dimensions: %dx%d", imageWidth, imageHeight);
        return;
    }

    // ============================
    // PNG
    // ============================
    if (format == ImageFormat::PNG) {
        // Only need first 24 bytes
        if (!loadData(24)) {
            ESP_LOGE(TAG, "Failed to load PNG header");
            return;
        }

        const uint8_t* p = reinterpret_cast<const uint8_t*>(data);

        // PNG signature check
        static const uint8_t pngSig[8] =
            { 0x89, 'P','N','G', 0x0D,0x0A,0x1A,0x0A };

        if (memcmp(p, pngSig, 8) != 0) {
            ESP_LOGE(TAG, "Invalid PNG signature");
            free(data);
            return;
        }

        // IHDR width/height (big-endian)
        imageWidth  = (p[16] << 24) | (p[17] << 16) | (p[18] << 8) | p[19];
        imageHeight = (p[20] << 24) | (p[21] << 16) | (p[22] << 8) | p[23];

        free(data);

        if (imageWidth <= 0 || imageHeight <= 0 ||
            imageWidth > 10000 || imageHeight > 10000) {
            ESP_LOGE(TAG, "Invalid PNG dimensions: %dx%d", imageWidth, imageHeight);
            return;
        }

        ESP_LOGI(TAG, "PNG dimensions: %dx%d", imageWidth, imageHeight);
        return;
    }
}


void Image::decodeAndScale(int targetW, int targetH)
{
    switch (format) {
        case ImageFormat::JPEG:
            decodeJPEGAndScale(targetW, targetH);
            break;

        case ImageFormat::PNG:
            decodePNGAndScale(targetW, targetH);
            break;

        default:
            ESP_LOGE(TAG, "Unsupported image format");
    }
}

void Image::decodeJPEGAndScale(int targetW, int targetH)
{
    constexpr size_t WORK_BUF_SIZE = 32000;
    static uint8_t work_buf[WORK_BUF_SIZE];

    JDEC jd;
    JRESULT res;

    char* data = nullptr;
    size_t size = 0;

    // ---------- Load JPEG ----------
    if (basePath.empty()) {
        FILE* file = fopen(imagePath.c_str(), "rb");
        if (!file) {
            ESP_LOGE(TAG, "Could not open JPEG: %s", imagePath.c_str());
            return;
        }

        fseek(file, 0, SEEK_END);
        size = ftell(file);
        fseek(file, 0, SEEK_SET);

        data = (char*)malloc(size);
        if (!data) {
            ESP_LOGE(TAG, "JPEG malloc failed (%zu bytes)", size);
            fclose(file);
            return;
        }

        fread(data, 1, size, file);
        fclose(file);
    } else {
        ZipFile zip(basePath.c_str());
        data = (char*)zip.read_file_to_memory(imagePath.c_str(), &size);
        if (!data) {
            ESP_LOGE(TAG, "JPEG read from zip failed");
            return;
        }
    }

    std::vector<uint8_t> decodeBuffer;

    JpegContext ctx {
        { reinterpret_cast<const uint8_t*>(data), size, 0 },
        { &decodeBuffer, 0, 0 }
    };

    // ---------- Prepare ----------
    res = jd_prepare(&jd, in_func, work_buf, WORK_BUF_SIZE, &ctx);
    if (res != JDR_OK) {
        ESP_LOGE(TAG, "JPEG prepare failed: %d", res);
        free(data);
        return;
    }

    int srcW = jd.width;
    int srcH = jd.height;

    // ---------- Choose TJpgDec scale ----------
    int scale = 0;
    while (scale < 3 &&
           (srcW >> (scale + 1)) >= targetW &&
           (srcH >> (scale + 1)) >= targetH) {
        scale++;
    }

    int outW = srcW >> scale;
    int outH = srcH >> scale;

    if (outW <= 0 || outH <= 0 ||
        outW > 10000 || outH > 10000 ||
        (size_t)outW * outH > 4'000'000) {
        ESP_LOGE(TAG, "Invalid JPEG decode size %dx%d", outW, outH);
        free(data);
        return;
    }

    // ---------- Allocate output ----------
    decodeBuffer.resize(outW * outH);
    ctx.out.img_w = outW;
    ctx.out.img_h = outH;

    // ---------- Decode ----------
    res = jd_decomp(&jd, out_func, scale);
    free(data);

    if (res != JDR_OK) {
        ESP_LOGE(TAG, "JPEG decode failed: %d", res);
        return;
    }

    imageWidth  = outW;
    imageHeight = outH;
    imageData.swap(decodeBuffer);

    // ---------- Optional final resize ----------
    if (imageWidth != targetW || imageHeight != targetH) {
        scaleImage(targetW, targetH);
    }

    ESP_LOGI(TAG, "JPEG decoded %dx%d (scale=%d)", imageWidth, imageHeight, scale);
}


void Image::decodePNGAndScale(int targetW, int targetH)
{
    char* data = nullptr;
    size_t size = 0;

    // --- Load PNG ---
    if (basePath.empty()) {
        FILE* f = fopen(imagePath.c_str(), "rb");
        if (!f) {
            ESP_LOGE(TAG, "Could not open PNG");
            return;
        }

        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fseek(f, 0, SEEK_SET);

        data = (char*)malloc(size);
        if (!data) {
            ESP_LOGE(TAG, "PNG malloc failed");
            fclose(f);
            return;
        }

        fread(data, 1, size, f);
        fclose(f);
    } else {
        ZipFile zip(basePath.c_str());
        data = (char*)zip.read_file_to_memory(imagePath.c_str(), &size);
        if (!data) {
            ESP_LOGE(TAG, "PNG read from zip failed");
            return;
        }
    }

    // --- Output buffer ---
    size_t outSize = targetW * imageHeight; // temporary: original height
    uint8_t* out = (uint8_t*)heap_caps_malloc(outSize, MALLOC_CAP_8BIT);
    if (!out) {
        ESP_LOGE(TAG, "PNG output alloc failed");
        free(data);
        return;
    }

    PNG png;

    // Set up context: horizontal scaling only
    PNGContext ctx {
        .out      = out,
        .outW     = targetW,       // horizontal scale
        .outH     = imageHeight,   // keep natural height for now
        .srcW     = imageWidth,
        .srcH     = imageHeight,
        .currentY = 0
    };

    s_pngCtx = &ctx;

    int rc = png.openRAM((uint8_t*)data, (int)size, pngLineCallback);
    if (rc != PNG_SUCCESS) {
        ESP_LOGE(TAG, "PNG open failed: %d", rc);
        s_pngCtx = nullptr;
        free(data);
        free(out);
        return;
    }

    png.decode(nullptr, 0);
    png.close();

    s_pngCtx = nullptr;
    free(data);

    // --- Copy decoded horizontal scale into imageData ---
    imageWidth  = targetW;
    imageHeight = ctx.outH;
    imageData.assign(out, out + targetW * ctx.outH);
    free(out);

    // --- Now scale vertically to targetH if needed ---
    if (imageHeight != targetH) {
        scaleImage(targetW, targetH);
    }

    ESP_LOGI(TAG, "PNG decoded %dx%d (final target %dx%d)", imageWidth, ctx.outH, targetW, targetH);
}




void Image::scaleImage(int targetW, int targetH) {
    if (imageData.empty() || imageWidth <= 0 || imageHeight <= 0) {
        ESP_LOGE(TAG, "scaleImage: no source image");
        return;
    }

    if (targetW <= 0 || targetH <= 0) {
        ESP_LOGE(TAG, "scaleImage: invalid target size");
        return;
    }

    const size_t newSize = (size_t)targetW * targetH;

    // Guard against insane allocations
    if (newSize > 4'000'000) {
        ESP_LOGE(TAG, "scaleImage: target too large (%zu bytes)", newSize);
        return;
    }

    uint8_t* dst = (uint8_t*)heap_caps_malloc(newSize, MALLOC_CAP_8BIT);
    if (!dst) {
        ESP_LOGE(TAG, "scaleImage: malloc failed (%dx%d)", targetW, targetH);
        return;
    }

    const int FP_SHIFT = 16;
    const int FP_ONE   = 1 << FP_SHIFT;

    int xRatio = ((imageWidth  - 1) << FP_SHIFT) / targetW;
    int yRatio = ((imageHeight - 1) << FP_SHIFT) / targetH;

    for (int y = 0; y < targetH; y++) {
        int srcY = (y * yRatio) >> FP_SHIFT;
        int yFrac = (y * yRatio) & (FP_ONE - 1);
        int dstRow = y * targetW;

        int srcRow = srcY * imageWidth;
        int srcRowNext = (srcY + 1 < imageHeight)
                           ? (srcY + 1) * imageWidth
                           : srcRow;

        for (int x = 0; x < targetW; x++) {
            int srcX = (x * xRatio) >> FP_SHIFT;
            int xFrac = (x * xRatio) & (FP_ONE - 1);

            int a = imageData[srcRow + srcX];
            int b = (srcX + 1 < imageWidth) ? imageData[srcRow + srcX + 1] : a;
            int c = imageData[srcRowNext + srcX];
            int d = (srcX + 1 < imageWidth) ? imageData[srcRowNext + srcX + 1] : c;

            int top    = a + (((b - a) * xFrac) >> FP_SHIFT);
            int bottom = c + (((d - c) * xFrac) >> FP_SHIFT);
            int value  = top + (((bottom - top) * yFrac) >> FP_SHIFT);

            dst[dstRow + x] = (uint8_t)value;
        }
    }

    // Commit atomically
    imageData.assign(dst, dst + newSize);
    free(dst);

    imageWidth  = targetW;
    imageHeight = targetH;

    ESP_LOGI(TAG, "Image scaled to %dx%d", targetW, targetH);
}


void Image::floydSteinbergDither() {
    if (imageData.empty() || imageWidth == 0 || imageHeight == 0) {
        ESP_LOGE(TAG, "No image data available for dithering");
        return;
    }

    const int width = imageWidth;
    const int height = imageHeight;

    // Use int16_t: enough range for error diffusion (-255..+255)
    std::vector<int16_t> currLine(width, 0);
    std::vector<int16_t> nextLine(width, 0);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // Current pixel value + propagated error
            int oldPixel = imageData[y * width + x] + currLine[x];

            // Clamp into 0–255 before thresholding
            if (oldPixel < 0)   oldPixel = 0;
            if (oldPixel > 255) oldPixel = 255;

            // Quantize to black/white
            int newPixel = (oldPixel < 128) ? 0 : 255;
            imageData[y * width + x] = static_cast<uint8_t>(newPixel);

            int error = oldPixel - newPixel;

            // Distribute error (with rounding instead of truncation)
            auto distribute = [&](int e, int num) {
                return static_cast<int16_t>((e * num + 8) >> 4); // divide by 16
            };


            if (x + 1 < width) {
                currLine[x + 1] += distribute(error, 7);
            }
            if (y + 1 < height) {
                if (x > 0) {
                    nextLine[x - 1] += distribute(error, 3);
                }
                nextLine[x] += distribute(error, 5);
                if (x + 1 < width) {
                    nextLine[x + 1] += distribute(error, 1);
                }
            }
        }

        // Move to next line
        currLine.swap(nextLine);
        std::fill(nextLine.begin(), nextLine.end(), 0);
    }
}

