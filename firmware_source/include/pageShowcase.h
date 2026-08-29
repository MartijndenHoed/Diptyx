#pragma once
#include <stdlib.h>

extern "C" {

// Display resolution
#define GLYPH_WIDTH 16
#define GLYPH_HEIGHT 16
}


class PageShowcase {
public:
    void init(void);
    const char* getPage();

    static const uint8_t* start();
    static const uint8_t* end();
    static size_t size();

private:
};