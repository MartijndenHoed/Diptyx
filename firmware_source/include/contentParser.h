#pragma once
#include <stdlib.h>
#include <vector>
#include "renderer.h"
#include "styleHandler.h"
#include "imageHandler.h"

extern "C" {

// Display resolution
#define GLYPH_WIDTH 16
#define GLYPH_HEIGHT 16
#define EPD_WIDTH       648
#define EPD_HEIGHT      480
}

#define LEFT_ALIGN 0
#define CENTERED 1
#define RIGHT_ALIGN 2

class ContentParser {
public:
    ContentParser(Renderer* renderer);
    ~ContentParser();
    void parseTextBlock(const std::vector<int> textBlock, bool newLine, Style style, bool draw);
    void parseImage(Image& image, Style& style,bool draw);
    void parseOverflowedImage(Style style,bool draw);
    void finishLine(bool draw, int align=LEFT_ALIGN);
    void finishPage(bool draw=false);
    void flushToOVerflowBuffer(std::vector<int> textBlock,int textBlockIndex);
    //int lineBuffer[60];
    //bool boldMask[60];
    //bool italicsMask[60];
    std::vector <int> textOverflowBuffer;
    Image *imageOverflowBuffer = nullptr;
    std::vector <int> lineBuffer;
    std::vector <int> boldMask;
    std::vector <int> italicsMask;
    int lineBufferWidth=0;
    int maxLines = 0;
    int fontHeight = 0;

    //int *textOverflow = nullptr;
    int currentLine;
    //int currentPos;
    int currentTextBlockIndex;
    Renderer *renderer = nullptr;
    int fontSize = 1;
    int textAlign = 0;
private:
};