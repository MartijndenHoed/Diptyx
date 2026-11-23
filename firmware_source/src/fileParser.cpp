#include "fileParser.h"
#include "Epub.h"
#include "TxtParser.h"
#include "esp_log.h"

static const char *TAG = "FILE_PARSER";

std::unique_ptr<FileParser> createFileParser(const std::string& path)
{
    std::string extension;

    size_t dot = path.find_last_of('.');
    if (dot != std::string::npos)
        extension = path.substr(dot);

    std::transform(
        extension.begin(),
        extension.end(),
        extension.begin(),
        [](unsigned char c) { return std::tolower(c); }
    );
    if (extension == ".epub")
        return std::make_unique<EpubParser>(path);

     if (extension == ".txt")
    return std::make_unique<TxtParser>(path);

    // if (extension == ".md" || extension == ".markdown")
    //     return std::make_unique<MarkdownParser>(path);
    ESP_LOGI(TAG, "Unsupported file type: %s",extension.c_str());
    return nullptr;
}

int FileParser::getCurrentPageAbsolute(int chapter, int page)
{
    int pageCounter = 0;
    for(int i = 0;i<chapter;i++)
    {
        pageCounter+= activeBook->chapterPageCounts[i];
    }
    pageCounter+=page;
    return pageCounter;
}

FileParser::FileParser(const std::string& bookPath)
{
}

FileParser::~FileParser() = default;