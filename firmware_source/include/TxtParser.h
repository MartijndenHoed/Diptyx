#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "fileParser.h"
#include <tinyxml2.h>
#include <string.h>
#include <renderer.h>
#include "imageHandler.h"
#include <contentParser.h>
#include "miniCSS.h"
#include <sstream>
#include <Epub.h>
#include <string_view>
#include "bookHandler.h"


class TxtParser: public FileParser
{
private:
std::string filePath;
int currentLineIndex = 0;

public:
  TxtParser(const std::string& m_path);
  ~TxtParser() {}
  int getCurrentPageInChapter(int currentPage) override;
  int getCurrentChapter(int currentPage) override;
  bool load(Book *activeBook) override;
  bool renderPageToBuffers(int pageNumber, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer, int pageCacheIndex=1) override;
  bool indexChapters() override;
  int parseTxtFile(char* content, size_t contentLength,int targetPage, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer,bool indexingMode);
  int getTOCLength() override;
  int getChapterIndexFromTOCindex(int TOCitemIndex) override;
  int getTocIndexFromChapterindex(int TOCitemIndex) override;
  std::string getPath() override;
  std::string getCoverImagePath() override;
  std::string getTOCItemTitle(int chapterIndex) override;

  ContentParser* contentParser = nullptr;
};