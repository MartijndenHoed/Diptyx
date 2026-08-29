#pragma once
#include <stdlib.h>
#include <string>
#include <vector>

extern "C" {

// Display resolution
#define GLYPH_WIDTH 16
#define GLYPH_HEIGHT 16
}


class FontHandler {
public:

    struct font {
        std::string fileName = "";
        std::string name = "";
        std::string family = "";
        int boundingBoxX = 0;
        int boundingBoxY = 0;
        int lineHeight = 0;
        int pointSize = 0;
        int shiftUp = 0;
    };

    struct fontFamily {
    std::string name;
    std::vector<font> fonts;
    };

    const char* getChar(int index);
    int getCharWidth(int index);
    int getFontCharWidth(int index);
    int getFontCharBitmapWidth(int index);
    int getFontCharHeight(int index);
    int getLineHeight();
    bool checkCharExist(int index);
    int getLeftBearing(int index);
    bool getFontChar(int index,int pixel);
    void testPrint(void);
    void indexFonts();
    void indexFont(const std::string &fontName);
    //void loadFont(const std::string &fontName);
    void loadFont(const std::string &yaffPath);
    int parseCodepoint(const std::string &tok);
    void processGlyph(int codepoint,
                               const std::vector<std::string> &bitmapLines,
                               size_t &pixelIndex,
                               int leftBearingVal,
                               int rightBearingVal);

    font currentFont;

    std::vector<uint8_t> width;
    std::vector<uint8_t> rightBearing;
    std::vector<uint8_t> leftBearing;
    std::vector<uint16_t> bitmapMap;
    std::vector<char> bitmap;

    std::vector <font> fonts;
    std::vector<fontFamily> families;
    static const uint8_t* startCharWidths();
    static const uint8_t* start();
    static const uint8_t* end();
    static size_t size();
    bool uniFontEnabled = false;


    char characterBuffer[32];

private:
    template <typename T>
    bool readFileToBuffer(const std::string &path, std::vector<T> &buffer);
};


