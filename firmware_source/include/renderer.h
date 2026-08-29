#pragma once
#include "epd5in83b_V2.h"
#include "imageHandler.h"
#include "string"
#include "fontHandler.h"



extern "C" {

// Display resolution
//#define GLYPH_WIDTH 8
//#define GLYPH_HEIGHT 16
//#define EPD_WIDTH       648
//#define EPD_HEIGHT      480

typedef unsigned int    UWORD;
typedef unsigned char   UBYTE;
typedef unsigned long   UDOUBLE;
}

int utf8ToCodePoints(const char* utf8, int* out, int maxOutLen);
std::vector<int> utf8ToCodePoints(const std::string& utf8);
std::vector<int> decodeHtmlEntities(const std::vector<int>& input);

class Renderer {
public:
    void init(void);
    void renderChecker(void);
    void drawScreen(bool screenID);
    void drawClearScreen(bool screenID);
    void drawSquare(int startx, int starty, int lengthx, int lengthy, bool value);
    void drawPaddedBox(int startx, int starty, int lengthx, int lengthy, int padding, bool value);
    void drawTextBox(int y, std::string line1, bool line1Bold,bool inverted=false);
    void drawTextBox(int y, std::string line1, bool line1Bold,std::string line2, bool line2Bold,bool inverted=false);
    void drawTextBox(int y, std::string line1, bool line1Bold,std::string line2, bool line2Bold,std::string line3, bool line3Bold,bool inverted=false);
    void drawPixel(int x, int y, bool value);
    int drawCharacter(int x, int y, int UTF8Index, bool bold=false,bool italics=false,int size=1,bool color = true, bool uniFont = true);
    void drawText(int x, int y, int* text, int text_length, int size, bool* boldMask=nullptr,bool* italicsMask=nullptr);
    void drawText(int x, int y, int* text, int text_length, int size, bool bold,bool italics);
    void drawString(int x, int y, std::string utf8String, int size, bool bold,bool italics,bool color = true);
    void drawString(int x, int y, std::vector<int> intArray,int size, std::vector<int> boldMask,std::vector<int> italicsMask,bool color = true, bool uniFont = true);
    void drawPageOverlay(unsigned char* framebuffer,int currentPage,int totalPages);
    void drawVoltage(unsigned char* framebuffer,float voltage);
    void drawBattery(unsigned char* framebuffer,int level);
    void drawImage(Image image, int x, int y);
    void drawBookMark(unsigned char* framebuffer,bool value);
    void drawProgressBar(int y,int length,int percent);
    void clearScreenBuffer(void);
    void clearScreenBuffer(unsigned char* framebuffer);
    void drawScreenPartial(bool screenID,int partialMinX,int partialMinY,int partialMaxX,int partialMaxY);
    void deepSleep(void);
    unsigned char* framebuffer;
    Epd epd;
    FontHandler fontHandler;
private:
    gpio_num_t ankerpin;
};