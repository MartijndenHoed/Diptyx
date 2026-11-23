#pragma once
#include <stdlib.h>
#include <string>
#include <vector>
#include <memory>
#include "bookHandler.h"

class FileParser {
public:
    FileParser(const std::string& path);
    ~FileParser();
    virtual bool load(Book *activeBook) = 0;
    virtual bool renderPageToBuffers(int pageNumber, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer, int pageCacheIndex=1) = 0;
    virtual bool indexChapters() = 0;

    virtual int getCurrentChapter(int currentPage) = 0;
    virtual int getCurrentPageInChapter(int currentPage) = 0;
    int getCurrentPageAbsolute(int chapter, int page);

    virtual int getTOCLength() = 0;
    virtual int getChapterIndexFromTOCindex(int TOCitemIndex) = 0;
    virtual int getTocIndexFromChapterindex(int chapterIndex) = 0;
    virtual std::string getTOCItemTitle(int TOCitemIndex) = 0;

    virtual std::string getCoverImagePath() = 0;
    virtual std::string getPath() = 0;

    std::vector <int> pageChapterIndex {0,0,0};
    std::vector <int> pageElementIndex {0,0,0};
    std::vector <bool> pageImagePresent {false,false,false};

    // std::string &get_path();
    // std::string &get_title();
    // std::string &get_author();
    // uint8_t *get_item_contents(const std::string &item_href, size_t *size = nullptr);

    Book *activeBook;

private:
}; 

std::unique_ptr<FileParser> createFileParser(const std::string& path);