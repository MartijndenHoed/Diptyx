#pragma once
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include "htmlParser.h"
#include <Epub.h>
#include "bookhandler.h"
#include "bookMarkMenuHandler.h"
extern "C" {

// Display resolution
#define GLYPH_WIDTH 16
#define GLYPH_HEIGHT 16
#define EPD_WIDTH       648
#define EPD_HEIGHT      480
}


class SimpleReader {
public:
    SimpleReader();
    ~SimpleReader();

    void init(std::string bookName,Renderer* renderer);
    void indexPages(void);
    int renderPages(unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer,int pageCacheIndex);
    std::string currentBookPath;
    std::string currentChapterPath;

    std::vector <int> pageChapterIndex {0,0,0};
    std::vector <int> pageElementIndex {0,0,0};
    std::vector <bool> pageImagePresent {false,false,false};
    bool imagePreviouslyPresent = false;

    //int currentPage = 0;
    int currentChapter = 0;
    //std::vector<int> chapterPageCounts;
    //int chapterCount = 0;
    //int totalPages = 0;
    int getCurrentPageAbsolute(int chapter, int page);
    int getCurrentChapter(int page);
    int getCurrentPageInChapter(int page);
    void nextPage(void);
    void prevPage(void);
    void openPage(void);
    void middleButtonAction();
    void rightPageAction();
    void leftPageAction();

    Epub *epub = nullptr;
    Renderer* renderer = nullptr;
    BookMarkMenuHandler *bookMarkMenuHandler = nullptr;
    Book *book = nullptr;

    unsigned char* leftPageFrameBuffer;
    unsigned char* rightPageFrameBuffer;
    unsigned char* leftPageFrameBufferNext;
    unsigned char* rightPageFrameBufferNext;
    unsigned char* leftPageFrameBufferPrevious;
    unsigned char* rightPageFrameBufferPrevious;
    //unsigned char* activeFrameBuffer;
private:
};