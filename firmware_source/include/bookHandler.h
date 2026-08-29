#pragma once
#include <string>
#include <list>
#include <vector>
#include <string.h>
#include <Epub.h>
#include <dirent.h>
#include "esp_log.h"
#include "renderer.h"
#include <map>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include "esp_littlefs.h"


#define MAX_BOOK_NAME 128
extern RTC_NOINIT_ATTR char rtc_currently_parsing_book[MAX_BOOK_NAME];


class Book {
public:
    struct RenderSettings {
        int fontSize = 1;
        int fontBold = 0;
        int marginsHorizontal = 1;
        int marginsVertical = 1;
        std::string fontFamily = "";
        int fontPoints = 0;
        int lineSpacing = 0;
    };

    struct BookMark {
        int pageIndex = 0;
        int chapterIndex = 0;
        int elementIndex = 0;
    };

    struct cachedImage {
        std::string filePath;
        int width = 0;
        int height = 0;
    };



    std::string path;
    std::string title;
    std::string author;
    int totalPageCount = 0;
    int chapterCount = 0;
    int currentPage = 0;
    int currentPageElementIndex = 0;
    int currentPageChapterIndex = 0;
    bool badParse = false;
    bool favorite = false;
    std::vector<int> chapterPageCounts;
    std::vector<BookMark> bookMarks;
    std::vector<cachedImage> cachedImages;
    RenderSettings renderSettings;

    Book(std::string path, std::string title, std::string author, int totalPageCount);

    Book(std::string path,
         std::string title,
         std::string author,
         int totalPageCount,
         int chapterCount,
         const std::vector<int>& chapterPageCounts,
         const RenderSettings& renderSettings);

    ~Book();

    // serialization
    std::string toJSON() const;
    static Book *fromJSON(const std::string &json);

    // compare render settings
    RenderSettings getCurrentRenderSettings();
    bool matchesRenderSettings(const RenderSettings &current) const;
};

class Author {
    public:
        Author(std::string name);
        std::vector<Book*> bookList;
        std::string name;
};



class BookHandler {
public:
    BookHandler();
    ~BookHandler();

    void initFilesystem();

    void listBooks(void);
    void updateReadPage(const std::string &bookPath, int newPageCount);

    void loadBooks(); // populate indexedBooks from memory
    void saveBook(Book *book); // write a book to memory
    void deleteBook(const std::string &bookPath); //delete book data from memory
    void transferDataToFlash(); //transfer all book data from sd to flash
    void transferDataToSD(); //transfer all book data from flash to sd
    void reindexBook(Book *book);
    void refreshFavorites();

    std::vector<Book*> bookList;

    std::vector<Author> authorList;
    std::map<std::string, Book*> indexedBooks;
private:
};

