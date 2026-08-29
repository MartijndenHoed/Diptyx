#pragma once
#include <string>
#include <list>
#include <vector>
#include <tinyxml2.h>
#include <string.h>
#include <renderer.h>
#include "imageHandler.h"
#include <contentParser.h>
#include <unordered_map>
#include "miniCSS.h"
#include <sstream>
#include <Epub.h>
#include "styleHandler.h"
#include <string_view>
#include "bookHandler.h"

class HtmlParser : public tinyxml2::XMLVisitor
{
private:
    std::string base_path;
    std::string styleSheetRef;
    CSSCache cssCache;
    int currentPage = 0;
    int targetPage = 0;
    Epub *epub = nullptr;
    unsigned char *leftScreenBuffer;
    unsigned char *rightScreenBuffer;



public:
    HtmlParser(const char *html, int length, const std::string &base_path, Renderer* renderer, int page, Epub *epub, unsigned char *leftScreenBuffer=nullptr, unsigned char *rightScreenBuffer=nullptr);
    ~HtmlParser();

    const char *htmlRaw;
    int htmlLength;

    bool VisitEnter(const tinyxml2::XMLElement &element, const tinyxml2::XMLAttribute *firstAttribute);
    bool Visit(const tinyxml2::XMLText &text);
    bool VisitExit(const tinyxml2::XMLElement &element);
    void parse();
    Renderer* renderer = nullptr;
    ContentParser* contentParser = nullptr;
    void emptyTextOverflow();
    bool isNewLine = true;
    bool precedingWhiteSpace = false;
    bool imagePresentOnPage = false;
    bool doneParsing = false;
    int pageReturn = 0;
    int currentElementIndex = 0;
    int currentChapterIndex = 0; //the current chapter
    int currentChapterStartPageIndex = 0; //at what page does the chapter start
    bool indexingMode = false; //are we currently indexing a book
    std::vector<Book::BookMark> bookMarks; //list of bookmarks for bookmark retention
    std::vector<Style> styleHierarchy; //the hierarchical list of styles
    std::vector<Book::cachedImage> cachedImages;
};
