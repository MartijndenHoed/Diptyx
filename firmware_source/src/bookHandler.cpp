#include "bookHandler.h"
#include "htmlParser.h"
#include "reader.h"
#include "cJSON.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string>
#include <map>
#include <unordered_map>
#include <algorithm>
#include "device.h"
#include "esp_log.h"
static const char *TAG = "Bookhandler";

RTC_NOINIT_ATTR char rtc_currently_parsing_book[MAX_BOOK_NAME] = {0};

bool ends_with_epub(const char *filename) {
    if (!filename || filename[0] == '.') {
        return false;   // reject hidden files
    }

    const char *ext = strrchr(filename, '.');
    return ext && strcasecmp(ext, ".epub") == 0;
}


bool rtc_isCurrentlyParsing(const std::string& bookPath)
{
    if (rtc_currently_parsing_book[0] == '\0') {
        return false;
    }
    return bookPath == std::string(rtc_currently_parsing_book);
}

void rtc_setCurrentlyParsing(const std::string& bookPath)
{
    std::strncpy(rtc_currently_parsing_book,
                 bookPath.c_str(),
                 MAX_BOOK_NAME - 1);
    rtc_currently_parsing_book[MAX_BOOK_NAME - 1] = '\0';
}

void rtc_clearCurrentlyParsing()
{
    rtc_currently_parsing_book[0] = '\0';
}

std::string rtc_getCurrentlyParsing()
{
    return std::string(rtc_currently_parsing_book);
}

// simple suffix check
static bool ends_with(const char *str, const char *suffix) {
    if (!str || !suffix) return false;
    size_t l1 = strlen(str);
    size_t l2 = strlen(suffix);
    if (l2 > l1) return false;
    return strcmp(str + l1 - l2, suffix) == 0;
}

// ensure the /bookStorage/books directory exists
static void ensureBooksDir() {
    const char *dirpath = "/littlefs/books";
    struct stat st;
    if (stat(dirpath, &st) != 0) {
        // doesn't exist -> try to create (recursive mkdir not required here since /bookStorage should exist)
        if (mkdir(dirpath, 0777) != 0) {
            ESP_LOGE(TAG, "mkdir(%s) failed: %s", dirpath, strerror(errno));
        } else {
            ESP_LOGI(TAG, "Created directory: %s", dirpath);
        }
    } else {
        if (!S_ISDIR(st.st_mode)) {
            ESP_LOGW(TAG, "%s exists but is not a directory", dirpath);
        } // else directory exists and is fine
    }
}

Book::Book(std::string path, std::string title, std::string author, int totalPageCount)
{
    this->author = std::move(author);
    this->title = std::move(title);
    this->path = std::move(path);
    this->currentPage = 0;
    this->totalPageCount = totalPageCount;
    this->chapterCount = 0;
    this->chapterPageCounts.clear();
    // renderSettings default constructed
}

Book::Book(std::string path,
           std::string title,
           std::string author,
           int totalPageCount,
           int chapterCount,
           const std::vector<int>& chapterPageCounts,
           const RenderSettings& renderSettings)
{
    this->path = std::move(path);
    this->title = std::move(title);
    this->author = std::move(author);
    this->totalPageCount = totalPageCount;
    this->chapterCount = chapterCount;
    this->chapterPageCounts = chapterPageCounts;
    this->currentPage = 0;
    this->renderSettings = renderSettings;
}

Book::~Book()
{
    // nothing special; BookHandler will own/free Book pointers
}

Author::Author(std::string name)
{
    this->name = name;
}

void BookHandler::initFilesystem() {
    //mount the bookstorage filesystem. book metadata is stored here
    esp_vfs_littlefs_conf_t conf = {  
        .base_path = "/littlefs",
        .partition_label = "bookStorage",
        .format_if_mount_failed = true,
        .read_only = false,
    };
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE("FS", "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE("FS", "Failed to find LittleFS partition");
        } else {
            ESP_LOGE("FS", "Failed to init LittleFS (%s)", esp_err_to_name(ret));
        }
    } else {
        size_t total, used;
        esp_littlefs_info("bookStorage", &total, &used);
        ESP_LOGI("FS", "LittleFS mounted. Partition size: total: %d, used: %d", total, used);
    }
    ensureBooksDir();

    //mount the assets filesystem. System epubs are stored here
    esp_vfs_littlefs_conf_t conf_2 = {
        .base_path = "/assets",
        .partition_label = "assets",
        .format_if_mount_failed = true,
        .read_only = false,
    };
    
    esp_err_t ret_2 = esp_vfs_littlefs_register(&conf_2);

    if (ret_2 != ESP_OK) {
        ESP_LOGE("FS", "assets mount failed: %s", esp_err_to_name(ret_2));
    } else {
        ESP_LOGI("FS", "assets mounted");
    }
}

std::string Book::toJSON() const {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "path", path.c_str());
    cJSON_AddStringToObject(root, "title", title.c_str());
    cJSON_AddStringToObject(root, "author", author.c_str());
    cJSON_AddNumberToObject(root, "totalPageCount", totalPageCount);
    cJSON_AddNumberToObject(root, "chapterCount", chapterCount);
    cJSON_AddNumberToObject(root, "currentPageChapterIndex", currentPageChapterIndex);
    cJSON_AddNumberToObject(root, "currentPageElementIndex", currentPageElementIndex);
    cJSON_AddNumberToObject(root, "currentPage", currentPage);
    cJSON_AddNumberToObject(root, "badParse", badParse);
    cJSON_AddNumberToObject(root, "favorite", favorite);

    // chapters
    cJSON *chapters = cJSON_CreateArray();
    for (int v : chapterPageCounts) {
        cJSON_AddItemToArray(chapters, cJSON_CreateNumber(v));
    }
    cJSON_AddItemToObject(root, "chapterPageCounts", chapters);

    // bookmarks (as arrays [page, chapter, element])
    cJSON *bmArray = cJSON_CreateArray();
    for (const BookMark &bm : bookMarks) {
        cJSON *arr = cJSON_CreateArray();
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.pageIndex));
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.chapterIndex));
        cJSON_AddItemToArray(arr, cJSON_CreateNumber(bm.elementIndex));
        cJSON_AddItemToArray(bmArray, arr);
    }
    cJSON_AddItemToObject(root, "bookMarks", bmArray);

    // render settings
    cJSON *r = cJSON_CreateObject();
    cJSON_AddNumberToObject(r, "fontSize", renderSettings.fontSize);
    cJSON_AddNumberToObject(r, "fontBold", renderSettings.fontBold);
    cJSON_AddNumberToObject(r, "marginsHorizontal", renderSettings.marginsHorizontal);
    cJSON_AddNumberToObject(r, "marginsVertical", renderSettings.marginsVertical);
    cJSON_AddNumberToObject(r, "fontPoints", renderSettings.fontPoints);
    cJSON_AddNumberToObject(r, "lineSpacing", renderSettings.lineSpacing);
    cJSON_AddStringToObject(r, "fontName", renderSettings.fontFamily.c_str());
    cJSON_AddItemToObject(root, "renderSettings", r);

    // cached images
    cJSON *imgArray = cJSON_CreateArray();
    for (const cachedImage &img : cachedImages) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "filePath", img.filePath.c_str());
        cJSON_AddNumberToObject(obj, "width", img.width);
        cJSON_AddNumberToObject(obj, "height", img.height);
        cJSON_AddItemToArray(imgArray, obj);
    }
    cJSON_AddItemToObject(root, "cachedImages", imgArray);

    // finalize
    char *jsonStr = cJSON_PrintUnformatted(root);
    std::string result = jsonStr ? std::string(jsonStr) : "{}";
    if (jsonStr) cJSON_free(jsonStr);
    cJSON_Delete(root);
    return result;
}


Book *Book::fromJSON(const std::string &json) {
    cJSON *root = cJSON_Parse(json.c_str());
    if (!root) return nullptr;

    cJSON *jpath = cJSON_GetObjectItem(root, "path");
    cJSON *jtitle = cJSON_GetObjectItem(root, "title");
    cJSON *jauthor = cJSON_GetObjectItem(root, "author");
    cJSON *jpages = cJSON_GetObjectItem(root, "totalPageCount");
    

    if (!cJSON_IsString(jpath) || !cJSON_IsString(jtitle) || !cJSON_IsString(jauthor) || !cJSON_IsNumber(jpages)) {
        cJSON_Delete(root);
        return nullptr;
    }

    Book *book = new Book(jpath->valuestring, jtitle->valuestring, jauthor->valuestring, jpages->valueint);

    cJSON *jchapterCount = cJSON_GetObjectItem(root, "chapterCount");
    cJSON *jchapterPages = cJSON_GetObjectItem(root, "chapterPageCounts");
    cJSON *jread = cJSON_GetObjectItem(root, "currentPage");
    cJSON *jreadChapter = cJSON_GetObjectItem(root, "currentPageChapterIndex");
    cJSON *jreadElement = cJSON_GetObjectItem(root, "currentPageElementIndex");
    cJSON *jrender = cJSON_GetObjectItem(root, "renderSettings");
    cJSON *jbadParse = cJSON_GetObjectItem(root, "badParse");
    cJSON *jfavorite = cJSON_GetObjectItem(root, "favorite");
    cJSON *jbookMarks = cJSON_GetObjectItem(root, "bookMarks");
    cJSON *jcachedImages = cJSON_GetObjectItem(root, "cachedImages");

    book->currentPageChapterIndex = cJSON_IsNumber(jreadChapter) ? jreadChapter->valueint : 0;
    book->currentPageElementIndex = cJSON_IsNumber(jreadElement) ? jreadElement->valueint : 0;
    book->chapterCount = cJSON_IsNumber(jchapterCount) ? jchapterCount->valueint : 0;
    book->currentPage = cJSON_IsNumber(jread) ? jread->valueint : 0;
    book->badParse = cJSON_IsNumber(jbadParse) ? jbadParse->valueint : false;
    book->favorite = cJSON_IsNumber(jfavorite) ? jfavorite->valueint : false;

    // chapterPageCounts
    if (jchapterPages && cJSON_IsArray(jchapterPages)) {
        int chapterLen = cJSON_GetArraySize(jchapterPages);
        for (int i = 0; i < chapterLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jchapterPages, i);
            book->chapterPageCounts.push_back(cJSON_IsNumber(item) ? item->valueint : 0);
        }
    }

    // bookmarks (supports both new array and old int formats)
    if (jbookMarks && cJSON_IsArray(jbookMarks)) {
        int bmLen = cJSON_GetArraySize(jbookMarks);
        for (int i = 0; i < bmLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jbookMarks, i);
            BookMark bm{};
            if (cJSON_IsArray(item)) {
                // new format [page, chapter, element]
                bm.pageIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 0)) ? cJSON_GetArrayItem(item, 0)->valueint : 0;
                bm.chapterIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 1)) ? cJSON_GetArrayItem(item, 1)->valueint : 0;
                bm.elementIndex = cJSON_IsNumber(cJSON_GetArrayItem(item, 2)) ? cJSON_GetArrayItem(item, 2)->valueint : 0;
            } else if (cJSON_IsNumber(item)) {
                // legacy integer format
                bm.pageIndex = item->valueint;
                bm.chapterIndex = 0;
                bm.elementIndex = 0;
            }
            book->bookMarks.push_back(bm);
        }
    }

    // cachedImages
    if (jcachedImages && cJSON_IsArray(jcachedImages)) {
        int imgLen = cJSON_GetArraySize(jcachedImages);
        for (int i = 0; i < imgLen; ++i) {
            cJSON *item = cJSON_GetArrayItem(jcachedImages, i);
            if (!cJSON_IsObject(item)) continue;

            cachedImage img{};

            cJSON *jp = cJSON_GetObjectItem(item, "filePath");
            cJSON *jw = cJSON_GetObjectItem(item, "width");
            cJSON *jh = cJSON_GetObjectItem(item, "height");

            if (cJSON_IsString(jp)) img.filePath = jp->valuestring;
            if (cJSON_IsNumber(jw)) img.width = jw->valueint;
            if (cJSON_IsNumber(jh)) img.height = jh->valueint;

            book->cachedImages.push_back(img);
        }
    }

    // renderSettings
    if (jrender && cJSON_IsObject(jrender)) {
        cJSON *jf = cJSON_GetObjectItem(jrender, "fontSize");
        cJSON *jb = cJSON_GetObjectItem(jrender, "fontBold");
        cJSON *mh = cJSON_GetObjectItem(jrender, "marginsHorizontal");
        cJSON *mv = cJSON_GetObjectItem(jrender, "marginsVertical");
        cJSON *ls = cJSON_GetObjectItem(jrender, "lineSpacing");
        cJSON *fp = cJSON_GetObjectItem(jrender, "fontPoints");
        cJSON *fn = cJSON_GetObjectItem(jrender, "fontName");
        if (cJSON_IsNumber(jf)) book->renderSettings.fontSize = jf->valueint;
        if (cJSON_IsNumber(jb)) book->renderSettings.fontBold = jb->valueint;
        if (cJSON_IsNumber(mh)) book->renderSettings.marginsHorizontal = mh->valueint;
        if (cJSON_IsNumber(mv)) book->renderSettings.marginsVertical = mv->valueint;
        if (cJSON_IsNumber(ls)) book->renderSettings.lineSpacing = ls->valueint;
        if (cJSON_IsNumber(fp)) book->renderSettings.fontPoints = fp->valueint;
        if (cJSON_IsString(fn)) book->renderSettings.fontFamily = fn->valuestring;
    }

    cJSON_Delete(root);
    return book;
}



bool Book::matchesRenderSettings(const RenderSettings &current) const {
    return renderSettings.fontSize == current.fontSize &&
           renderSettings.fontBold == current.fontBold &&
           renderSettings.marginsHorizontal == current.marginsHorizontal &&
           renderSettings.marginsVertical == current.marginsVertical &&
           renderSettings.lineSpacing == current.lineSpacing &&
           renderSettings.fontPoints == current.fontPoints &&
           renderSettings.fontFamily == current.fontFamily;
}

Book::RenderSettings Book::getCurrentRenderSettings() {
    Device &dev = Device::getInstance();
    RenderSettings currentRS {
            dev.renderSettings.fontSize,
            dev.renderSettings.fontBold,
            dev.renderSettings.marginsHorizontal,
            dev.renderSettings.marginsVertical,
            dev.renderer->fontHandler.families[dev.renderSettings.fontFamily].name,
            dev.renderSettings.fontPoints,
            dev.renderSettings.lineSpacing,
        };
    return currentRS;
}

BookHandler::BookHandler()
{
    initFilesystem();
}

BookHandler::~BookHandler()
{
    // free all owned Book pointers
    for (auto &kv : indexedBooks) {
        delete kv.second;
    }
    indexedBooks.clear();
    // bookList contains pointers owned above, so clear it (no double delete)
    bookList.clear();
    authorList.clear();
}



void BookHandler::loadBooks()
{
    // Free existing books
    for (auto &kv : indexedBooks) {
        delete kv.second;
    }
    indexedBooks.clear();

    bool loadFromSD = Device::getInstance().deviceSettings.storeDataOnSD;
    const char *basePath = loadFromSD ? "/sdcard/book_data" : "/littlefs/books";

    DIR *dir = opendir(basePath);
    if (!dir) {
        ESP_LOGW(TAG, "No book storage directory found: %s", basePath);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string fullPath = std::string(basePath) + "/" + filename;

        FILE *f = fopen(fullPath.c_str(), "r");
        if (!f) {
            ESP_LOGW(TAG, "Failed to open %s", fullPath.c_str());
            continue;
        }

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (size <= 0) {
            fclose(f);
            continue;
        }

        std::vector<char> buffer(size + 1);
        fread(buffer.data(), 1, size, f);
        fclose(f);
        buffer[size] = '\0';

        Book *book = Book::fromJSON(buffer.data());
        if (book) {
            indexedBooks[book->path] = book;
        } else {
            ESP_LOGW(TAG, "Invalid book JSON: %s", fullPath.c_str());
        }
    }

    closedir(dir);

    ESP_LOGI(
        TAG,
        "Loaded %d books from %s",
        indexedBooks.size(),
        loadFromSD ? "SD card" : "flash"
    );
}


// save this->indexedBooks to NVS
void BookHandler::saveBook(Book *book)
{
    if (!book) return;

    const char *basePath = Device::getInstance().deviceSettings.storeDataOnSD ? "/sdcard/book_data" : "/littlefs/books";

    // Ensure directory exists
    struct stat st;
    if (stat(basePath, &st) != 0) {
        if (mkdir(basePath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create directory: %s", basePath);
            return;
        }
    }

    std::string filename = std::string(basePath) + "/" + book->path + ".json";
    std::string jsonStr = book->toJSON();

    FILE *f = fopen(filename.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for writing", filename.c_str());
        return;
    }

    size_t written = fwrite(jsonStr.c_str(), 1, jsonStr.size(), f);
    fclose(f);

    if (written != jsonStr.size()) {
        ESP_LOGE(TAG, "Short write when saving book metadata: %s", filename.c_str());
        return;
    }

    ESP_LOGI(
        TAG,
        "Saved book metadata (%s): %s",
        Device::getInstance().deviceSettings.storeDataOnSD ? "SD" : "FLASH",
        filename.c_str()
    );
}


void BookHandler::deleteBook(const std::string &bookPath)
{
    bool deleteFromSD = Device::getInstance().deviceSettings.storeDataOnSD;
    const char *basePath = deleteFromSD ? "/sdcard/book_data" : "/littlefs/books";

    std::string filename = std::string(basePath) + "/" + bookPath + ".json";

    if (remove(filename.c_str()) != 0) {
        ESP_LOGW(TAG, "Failed to delete book file: %s", filename.c_str());
    } else {
        ESP_LOGI(TAG, "Deleted book file: %s", filename.c_str());
    }

    auto it = indexedBooks.find(bookPath);
    if (it != indexedBooks.end()) {
        delete it->second;
        indexedBooks.erase(it);
    }
}

void BookHandler::transferDataToSD()
{
    const char *flashPath = "/littlefs/books";
    const char *sdPath    = "/sdcard/book_data";

    // Ensure SD directory exists
    struct stat st;
    if (stat(sdPath, &st) != 0) {
        if (mkdir(sdPath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create SD book_data directory");
            return;
        }
    }

    DIR *dir = opendir(flashPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open flash books directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string srcPath  = std::string(flashPath) + "/" + filename;
        std::string destPath = std::string(sdPath)    + "/" + filename;

        FILE *src = fopen(srcPath.c_str(), "r");
        if (!src) {
            ESP_LOGW(TAG, "Failed to open source file: %s", srcPath.c_str());
            continue;
        }

        FILE *dst = fopen(destPath.c_str(), "w");
        if (!dst) {
            ESP_LOGW(TAG, "Failed to open destination file: %s", destPath.c_str());
            fclose(src);
            continue;
        }

        char buffer[512];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        ESP_LOGI(TAG, "Transferred book metadata: %s", filename.c_str());
    }

    closedir(dir);

    ESP_LOGI(TAG, "Book data transfer from flash to SD completed");
}

void BookHandler::transferDataToFlash()
{
    const char *sdPath    = "/sdcard/book_data";
    const char *flashPath = "/littlefs/books";

    // Ensure flash directory exists
    struct stat st;
    if (stat(flashPath, &st) != 0) {
        if (mkdir(flashPath, 0777) != 0) {
            ESP_LOGE(TAG, "Failed to create flash books directory");
            return;
        }
    }

    DIR *dir = opendir(sdPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open SD book_data directory");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) continue;

        std::string filename(entry->d_name);
        if (!ends_with(filename.c_str(), ".json")) continue;

        std::string srcPath  = std::string(sdPath)    + "/" + filename;
        std::string destPath = std::string(flashPath) + "/" + filename;

        FILE *src = fopen(srcPath.c_str(), "r");
        if (!src) {
            ESP_LOGW(TAG, "Failed to open source file: %s", srcPath.c_str());
            continue;
        }

        FILE *dst = fopen(destPath.c_str(), "w");
        if (!dst) {
            ESP_LOGW(TAG, "Failed to open destination file: %s", destPath.c_str());
            fclose(src);
            continue;
        }

        char buffer[512];
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
            fwrite(buffer, 1, bytes, dst);
        }

        fclose(src);
        fclose(dst);

        ESP_LOGI(TAG, "Transferred book metadata: %s", filename.c_str());
    }

    closedir(dir);

    ESP_LOGI(TAG, "Book data transfer from SD to flash completed");
}


void BookHandler::listBooks(void)
{
    if(authorList.empty()) authorList.emplace_back("Favorite books");
    const char *sdPath = "/sdcard";

    DIR *dir = opendir(sdPath);
    if (!dir) {
        ESP_LOGE(TAG, "Failed to open directory: %s", sdPath);
        return;
    }

    Device &dev = Device::getInstance();
    if (dev.activeBookPath.empty()) dev.activeBookIndex = 0;

    // Step 1: Load all cached books from LittleFS
    loadBooks();  // fills indexedBooks with any JSON files found

    // Build current render settings snapshot
    Book::RenderSettings currentRS {
        dev.renderSettings.fontSize,
        dev.renderSettings.fontBold,
        dev.renderSettings.marginsHorizontal,
        dev.renderSettings.marginsVertical
    };

    // Step 2: Scan /sdcard and process each EPUB
    std::vector<std::string> foundPaths;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR) continue;
        if (!ends_with_epub(entry->d_name)) continue;
        std::string fileName(entry->d_name);
        foundPaths.push_back(fileName);

        Book *book = nullptr;
        auto it = indexedBooks.find(fileName);

        if (it != indexedBooks.end()) {
            // Cached book exists
            book = it->second;
            this->bookList.push_back(book);  
        } else {
            // New book -> index and save
            std::string fullPath = std::string(sdPath) + "/" + fileName;
            if (rtc_isCurrentlyParsing(fileName)) { //if the book is malformed and somehow crashes the loader, this is triggered
                //notify the user about the error, and store the epub as malformed
                dev.notificationHandler->drawErrorNotification(fileName);
                book = new Book(fileName,
                                    fileName,
                                    std::string("error during parsing"),
                                    0,
                                    0,
                                    std::vector<int> {},
                                    currentRS);
                book->badParse = true;
                this->bookList.push_back(book);
                indexedBooks[fileName] = book;
                // Save immediately to LittleFS
                saveBook(book);
                rtc_clearCurrentlyParsing();
                vTaskDelay(100);
            }
            else
            {
                rtc_setCurrentlyParsing(fileName);
                Epub *epub = new Epub(fullPath);
                ESP_LOGI(TAG, "Indexing new book: %s", fullPath.c_str());
                vTaskDelay(20);

                if (epub->load()) {
                    std::string title = epub->get_title();
                    std::string author = epub->get_author();
                    dev.notificationHandler->drawIndexingNotification(title);

                    Reader reader;
                    book = new Book(fileName,
                                    title,
                                    author,
                                    0,
                                    0,
                                    std::vector<int> {},
                                    currentRS);
                    book->renderSettings.fontSize=-1; //change this to some bad value so it is forced to re-index on opening
                    //reader.init(book, nullptr);
                    //reader.indexPages(); //don't init here actually, it is done upon first opening
                    this->bookList.push_back(book);
                    indexedBooks[fileName] = book;

                    // Save immediately to LittleFS
                    saveBook(book);
                }
                else
                {
                    //if the epub loading is unsuccessful, we also store the epub as malformed and inform the user
                    dev.notificationHandler->drawErrorNotification(fileName);
                    book = new Book(fileName,
                                        fileName,
                                        std::string("error during parsing"),
                                        0,
                                        0,
                                        std::vector<int> {},
                                        currentRS);
                    book->badParse = true;
                    this->bookList.push_back(book);
                    indexedBooks[fileName] = book;
                    // Save immediately to LittleFS
                    saveBook(book);
                    vTaskDelay(100);
                }
                delete epub;
                rtc_clearCurrentlyParsing();
            }
        }

        if (dev.activeBookPath.empty()) dev.activeBookPath = fileName;
    }
    closedir(dir);

    // Step 3: Prune metadata for missing books (not found in /sdcard)
    for (auto it = indexedBooks.begin(); it != indexedBooks.end();) {
        if (std::find(foundPaths.begin(), foundPaths.end(), it->first) == foundPaths.end()) {
            // Remove JSON file
            std::string metaFile = std::string("/bookStorage/books/") + it->first + ".json";
            unlink(metaFile.c_str());
            delete it->second;
            it = indexedBooks.erase(it);
            ESP_LOGI(TAG, "Pruned stale metadata for %s", metaFile.c_str());
        } else {
            ++it;
        }
    }

    // Step 4: Sort books into authorList
    for (Book* b : bookList) {
        Author* foundAuthor = nullptr;
        int currentAuthorIndex = 0;
        for (auto& a : authorList) {
            if (a.name == b->author) {
                foundAuthor = &a;
                break;
            }
            currentAuthorIndex++;
        }

        if (foundAuthor) {
            foundAuthor->bookList.push_back(b);
            if (b->author == dev.activeAuthorName) {
                dev.activeAuthorIndex = currentAuthorIndex;
            }
        } else {
            authorList.emplace_back(b->author);
            authorList.back().bookList.push_back(b);
            if (b->author == dev.activeAuthorName) {
                dev.activeAuthorIndex = authorList.size() - 1;
            }
            foundAuthor = &authorList.back();
        }

        if (dev.activeBookPath == b->path) {
            dev.activeBookIndex = foundAuthor->bookList.size() - 1;
            if(dev.activeAuthorIndex==-1) //this book is the current book, but it's author doesn't match the current author, so it is in the favorites
            {
                dev.activeBookIndex = authorList[0].bookList.size(); //the book is the last entry in the favorites list, but not yet added, so size+1-1
                dev.activeAuthorIndex = 0;
                dev.activeAuthorName = authorList[0].name;
            }
        }

        if(b->favorite)
        {
            authorList[0].bookList.push_back(b);
        }
    }

    if(dev.state == Device::State::simpleReader) 
    {
        dev.activeBookIndex = 0;
        dev.activeAuthorIndex = 0;
        return; //if we boot into the simplereader we don't need to check the activebook
    }

    // Step 5: Fallback active indices if missing
    if (dev.activeBookIndex == -1 && !bookList.empty()) {
        dev.activeBookIndex = 0;
        dev.activeBookPath = bookList[0]->path;
    }
    if (dev.activeAuthorIndex == -1 && !authorList.empty()) {
        dev.activeAuthorIndex = 0;
        dev.activeAuthorName = authorList[0].name;
    }
}

void BookHandler::refreshFavorites()
{
    authorList[0].bookList.clear();
    for (Book* b : bookList) {
        if(b->favorite)
        {
            authorList[0].bookList.push_back(b);
        }
    }
}

void BookHandler::updateReadPage(const std::string &bookPath, int newPageCount) {
    auto it = indexedBooks.find(bookPath);
    if (it == indexedBooks.end()) return;

    Book *book = it->second;
    book->currentPage = newPageCount;

    saveBook(book); // only update this book's file
}

void BookHandler::reindexBook(Book *book) {
    if (!book) return;

    Reader reader;
    reader.init(book, nullptr);
    reader.indexPages();
    book->renderSettings = book->getCurrentRenderSettings();

    saveBook(book); // only update this book's file
    ESP_LOGI(TAG, "Re-indexed and saved book: %s", book->title.c_str());
}

