#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_log.h>
#include <map>
#include "tinyxml2.h"
#include "ZipFile.h"
#include "TxtParser.h"
#include "device.h"
#include <cstdio>
#include "styleHandler.h"

static const char *TAG = "TXT_PARSER";

char* readTxtFile(const char* path, size_t& fileSize)
{
    FILE* file = fopen(path, "rb");
    if (file == nullptr)
        return nullptr;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);

    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return nullptr;
    }

    char* content = new char[size + 1];

    size_t bytesRead = fread(content, 1, size, file);
    fclose(file);

    if (bytesRead != static_cast<size_t>(size)) {
        delete[] content;
        return nullptr;
    }

    content[size] = '\0'; // Null-terminate the text
    fileSize = size;

    return content;
}

TxtParser::TxtParser(const std::string &path)
    : FileParser(path),
      filePath(path)
{
}

int TxtParser::getCurrentPageInChapter(int currentPage)
{
    return currentPage;
}

int TxtParser::getCurrentChapter(int currentPage)
{
    return 0;
}

bool TxtParser::load(Book *activeBook)
{
    this->activeBook = activeBook;
    activeBook->author = "Text Files";
    activeBook->title = activeBook->fileName;
    activeBook->path = filePath;
    return true;
}

int TxtParser::getTOCLength()
{
    return 0;
}

int TxtParser::getChapterIndexFromTOCindex(int TOCitemIndex)
{
    return 0;
}

int TxtParser::getTocIndexFromChapterindex(int TOCitemIndex)
{
    return 0;
}

std::string TxtParser::getPath() {
  return filePath;
}

std::string TxtParser::getCoverImagePath() {
  return "";
}

std::string TxtParser::getTOCItemTitle(int chapterIndex) {
  return filePath;
}


bool TxtParser::renderPageToBuffers(int pageNumber, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer, int pageCacheIndex)
{
    size_t fileSize = 0;
    char* content = readTxtFile(filePath.c_str(),fileSize);
    if(content)
    {
        Device::getInstance().renderer->clearScreenBuffer(leftScreenBuffer);
        Device::getInstance().renderer->clearScreenBuffer(rightScreenBuffer);
        HtmlParser *parser = nullptr;
        parseTxtFile(content, fileSize,pageNumber, leftScreenBuffer, rightScreenBuffer,false);
        //pageElementIndex[pageCacheIndex] = parser->currentElementIndex;
        pageChapterIndex[pageCacheIndex] = this->currentLineIndex;
        pageImagePresent[pageCacheIndex] = false;
        delete[] content;
        return true;
    }

    return false;
}

bool TxtParser::indexChapters() {
    //txt files don't really need indexing; the file is a single chapter
    activeBook->chapterPageCounts.clear();
    activeBook->cachedImages.clear();
    activeBook->totalPageCount = 0;
    activeBook->chapterCount = 0;
    
    size_t fileSize = 0;
    char* content = readTxtFile(filePath.c_str(),fileSize);
    if(content)
    {
        HtmlParser *parser = nullptr;
        int parsedPages = parseTxtFile(content, fileSize,-2, nullptr, nullptr,true); 
        parsedPages += 1;
        if(parsedPages%2) parsedPages+=1;
        activeBook->totalPageCount = parsedPages;
        activeBook->chapterPageCounts.push_back(parsedPages);
        //pageElementIndex[pageCacheIndex] = parser->currentElementIndex;
        delete[] content;
    }
    return true;
}

int TxtParser::parseTxtFile(char* content, size_t contentLength,int targetPage, unsigned char *leftScreenBuffer, unsigned char *rightScreenBuffer,bool indexingMode) {
    const char* line = content;
    const char* lineEnd;
    currentLineIndex = 0;
    int currentPage = 0;
    bool doneParsing = false;
    contentParser = new ContentParser(Device::getInstance().renderer);
    Device::getInstance().renderer->framebuffer=leftScreenBuffer;
    Style style = Style();

    while (!doneParsing && *line != '\0') {
        lineEnd = strchr(line, '\n');
        size_t lineLength = lineEnd ? (size_t)(lineEnd - line) : strlen(line);

        if (lineLength > 0 && line[lineLength - 1] == '\r') {
            --lineLength;
        }
        bool isNewLine = true;
        bool drawPage = (currentPage==targetPage)||(currentPage==targetPage+1);
        std::string lineString(line,lineLength);
        std::vector<int> intArray;
        intArray = utf8ToCodePoints(lineString);
        contentParser->parseTextBlock(intArray,isNewLine,style,drawPage);
        isNewLine = false;
        while(contentParser->textOverflowBuffer.size()) 
        {
            currentPage++;
            if(currentPage==targetPage+1) Device::getInstance().renderer->framebuffer=rightScreenBuffer;
            if(currentPage>targetPage+1 && !indexingMode) doneParsing = true;
                drawPage = (currentPage==targetPage)||(currentPage==targetPage+1);
                std::vector <int> textOverflowBufferCopy = contentParser->textOverflowBuffer;
                contentParser->textOverflowBuffer.clear();
                this->contentParser->parseTextBlock(textOverflowBufferCopy,false,style,drawPage);
        }
        contentParser->finishLine(drawPage,style.align);

        
        currentLineIndex++;
        if (lineEnd == nullptr) break; // Last line has been processed

        line = lineEnd + 1;
    }

    // Process the final line
    if (*line != '\0') {
        size_t lineLength = strlen(line);
    }
    bool drawPage = (currentPage==targetPage)||(currentPage==targetPage+1);
    contentParser->finishLine(drawPage);
    contentParser->finishPage(drawPage);
    if(indexingMode) activeBook->wordCount = contentParser->wordCount;

    delete contentParser;
    contentParser = nullptr;
    return currentPage;
}