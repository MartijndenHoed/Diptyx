#include "simpleReader.h"
#include "esp_log.h"
#include <stdio.h>
#include <dirent.h>
#include <string>
#include <vector>
#include "driver/adc.h"
#include <functional>
#include "device.h"
#include "esp_timer.h"
#include "menuHandler.h"
#include "reader.h"

static const char *TAG = "simpleReader";

SimpleReader::SimpleReader()
{
    leftPageFrameBuffer = Device::getInstance().reader->leftPageFrameBuffer;
    leftPageFrameBufferNext = Device::getInstance().reader->leftPageFrameBufferNext;
    leftPageFrameBufferPrevious = Device::getInstance().reader->leftPageFrameBufferPrevious;
    rightPageFrameBuffer = Device::getInstance().reader->rightPageFrameBuffer;
    rightPageFrameBufferNext = Device::getInstance().reader->rightPageFrameBufferNext;
    rightPageFrameBufferPrevious = Device::getInstance().reader->rightPageFrameBufferPrevious;
}

SimpleReader::~SimpleReader()
{
    free(leftPageFrameBuffer);
    free(leftPageFrameBufferNext);
    free(leftPageFrameBufferPrevious);
    free(rightPageFrameBuffer);
    free(rightPageFrameBufferNext);
    free(rightPageFrameBufferPrevious);
    if (epub) {
    delete epub;
    }
}


void SimpleReader::init(std::string bookName,Renderer* renderer)
{
    this->renderer = renderer;
    this->currentBookPath = bookName;
    this->currentChapter = 0;

    this->epub = new Epub(std::string("/assets/") + this->currentBookPath);
    ESP_LOGI(TAG, "book path: %s", this->currentBookPath.c_str());
    Device::getInstance().notificationHandler->drawNotification("Opening " + this->currentBookPath);
    if (epub->load())
    {
        ESP_LOGI(TAG, "book title: %s", (epub->get_title()).c_str());
        ESP_LOGI(TAG, "first chapter: %s", (epub->get_spine_item(0)).c_str());
    }
    else
    {
        Device::getInstance().notificationHandler->drawNotification("Error opening " + this->currentBookPath);
        Device::getInstance().state=Device::State::Menu;
        renderer->epd.forceRefresh();

        Device::getInstance().menuHandler->drawMenu();
        Device::getInstance().saveAppState();
        return;
    }
    std::string title = epub->get_title();
    std::string author = epub->get_author();

    book = new Book(currentBookPath,
                    title,
                    author,
                    0);
    indexPages();
    openPage();
}

int SimpleReader::getCurrentPageAbsolute(int chapter, int page)
{
    int pageCounter = 0;
    for(int i = 0;i<chapter;i++)
    {
        pageCounter+= book->chapterPageCounts[i];
    }
    pageCounter+=page;
    return pageCounter;
}

int SimpleReader::getCurrentChapter(int page)
{
    int pageCounter = page;
    for(int i = 0;i<this->epub->get_spine_items_count();i++)
    {
        pageCounter-= book->chapterPageCounts[i];
        if(pageCounter<0) return i;
    }
    return 0;
}

int SimpleReader::getCurrentPageInChapter(int page)
{
    int pageCounter = page;
    for(int i = 0;i<this->epub->get_spine_items_count();i++)
    {
        if(pageCounter<book->chapterPageCounts[i]) return pageCounter;
        pageCounter-= book->chapterPageCounts[i];
    }
    return 0;
}


int SimpleReader::renderPages(unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer,int pageCacheIndex)
{
    ESP_LOGI(TAG, "rendering page %d", book->currentPage);
    currentChapter = getCurrentChapter(book->currentPage);
    int currentPageInChapter = getCurrentPageInChapter(book->currentPage);
    std::string currentChapterPath = epub->get_spine_item(this->currentChapter);
    ESP_LOGI(TAG, "render Chapter: %s", currentChapterPath.c_str());
    char *html = reinterpret_cast<char *>(this->epub->get_item_contents(currentChapterPath));
    if(html)
    {
        renderer->clearScreenBuffer(leftPageFrameBuffer);
        renderer->clearScreenBuffer(rightPageFrameBuffer);
        HtmlParser *parser = nullptr;
        parser = new HtmlParser(html, strlen(html), currentChapterPath,this->renderer,currentPageInChapter,this->epub,leftPageFrameBuffer,rightPageFrameBuffer);
        parser->cachedImages = book->cachedImages;
        parser->parse();
        pageElementIndex[pageCacheIndex] = parser->currentElementIndex;
        pageChapterIndex[pageCacheIndex] = currentChapter;
        pageImagePresent[pageCacheIndex] = parser->imagePresentOnPage;

        free(html);
        delete parser;
        this->renderer->drawPageOverlay(leftPageFrameBuffer,book->currentPage+1,book->totalPageCount);
        this->renderer->drawPageOverlay(rightPageFrameBuffer,book->currentPage+2,book->totalPageCount);
        // if (Device::getInstance().deviceSettings.displayBattery) {
        //     int batteryLevel = Device::getInstance().getBatteryPercentage();;
        //     this->renderer->drawBattery(rightPageFrameBuffer,batteryLevel);
        // }
        if(book->currentPage==0) this->renderer->epd.forceRefresh();
    }
    return 0;
}

void SimpleReader::openPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(50000);
    renderPages(leftPageFrameBuffer, rightPageFrameBuffer,1);
    if((imagePreviouslyPresent!=pageImagePresent[1]) || pageImagePresent[1]) renderer->epd.forceRefresh();
    imagePreviouslyPresent=pageImagePresent[1];
    renderer->drawBattery(rightPageFrameBuffer,Device::getInstance().getBatteryPercentage());
    this->renderer->epd.DisplayPictureBoth(
        leftPageFrameBuffer,
        rightPageFrameBuffer,
        [this]() {
            book->currentPage += 2;
            renderPages(leftPageFrameBufferNext, rightPageFrameBufferNext,2);
            book->currentPage -= 2;

            if (book->currentPage >= 2) {
                book->currentPage -= 2;
                renderPages(leftPageFrameBufferPrevious, rightPageFrameBufferPrevious,0);
                book->currentPage += 2;
            }
        }
    );
    book->currentPageElementIndex = pageElementIndex[1]; //store what chapter and html element the current page is at
    book->currentPageChapterIndex = pageChapterIndex[1];
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(pageImagePresent[0]),
         static_cast<int>(pageImagePresent[1]),
         static_cast<int>(pageImagePresent[2]));
    
    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
}

void SimpleReader::nextPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(50000);
    if(book->currentPage >= book->totalPageCount-2)
    {
        if (epub) {
            delete epub;
        }

        if(book) {
            delete book;
        }
        Device::getInstance().state=Device::State::Menu;
        renderer->epd.forceRefresh();

        Device::getInstance().menuHandler->drawMenu();
        Device::getInstance().saveAppState();
        return;
    }
    book->currentPage += 2;


    renderer->drawBattery(rightPageFrameBufferNext,Device::getInstance().getBatteryPercentage());
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(pageImagePresent[0]),
         static_cast<int>(pageImagePresent[1]),
         static_cast<int>(pageImagePresent[2]));
    std::rotate(pageChapterIndex.begin(), pageChapterIndex.begin() + 1, pageChapterIndex.end());
    std::rotate(pageElementIndex.begin(), pageElementIndex.begin() + 1, pageElementIndex.end());
    std::rotate(pageImagePresent.begin(), pageImagePresent.begin() + 1, pageImagePresent.end());
    if((imagePreviouslyPresent!=pageImagePresent[1]) || pageImagePresent[1]) 
    {
        if( (100*abs(countBlackPixels(leftPageFrameBufferNext)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
        (100*abs(countBlackPixels(rightPageFrameBufferNext)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
        {
            renderer->epd.forceRefresh();
        }
    }
    imagePreviouslyPresent=pageImagePresent[1];

    this->renderer->epd.DisplayPictureBoth(
        leftPageFrameBufferNext,
        rightPageFrameBufferNext,
        [this]() {
            memcpy(leftPageFrameBufferPrevious, leftPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
            memcpy(rightPageFrameBufferPrevious, rightPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
            memcpy(leftPageFrameBuffer, leftPageFrameBufferNext, EPD_WIDTH * EPD_HEIGHT / 8);
            memcpy(rightPageFrameBuffer, rightPageFrameBufferNext, EPD_WIDTH * EPD_HEIGHT / 8);
            book->currentPage += 2;
            renderPages(leftPageFrameBufferNext, rightPageFrameBufferNext,2);
            book->currentPage -= 2;
        }
    );
    book->currentPageElementIndex = pageElementIndex[1]; //store what chapter and html element the current page is at
    book->currentPageChapterIndex = pageChapterIndex[1];
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(pageImagePresent[0]),
         static_cast<int>(pageImagePresent[1]),
         static_cast<int>(pageImagePresent[2]));

    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
}

void SimpleReader::prevPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(50000);
    if(book->currentPage>=2)
    {
        book->currentPage -= 2;
        renderer->drawBattery(rightPageFrameBufferPrevious,Device::getInstance().getBatteryPercentage());
        ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(pageImagePresent[0]),
         static_cast<int>(pageImagePresent[1]),
         static_cast<int>(pageImagePresent[2]));
        std::rotate(pageChapterIndex.begin(), pageChapterIndex.begin() + 2, pageChapterIndex.end());
        std::rotate(pageElementIndex.begin(), pageElementIndex.begin() + 2, pageElementIndex.end());
        std::rotate(pageImagePresent.begin(), pageImagePresent.begin() + 2, pageImagePresent.end());
        if((imagePreviouslyPresent!=pageImagePresent[1]) || pageImagePresent[1]) 
        {
            if( (100*abs(countBlackPixels(leftPageFrameBufferPrevious)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
            (100*abs(countBlackPixels(rightPageFrameBufferPrevious)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
            {
                renderer->epd.forceRefresh();
            }     
        }
        imagePreviouslyPresent=pageImagePresent[1];

        this->renderer->epd.DisplayPictureBoth(
            leftPageFrameBufferPrevious,
            rightPageFrameBufferPrevious,
            [this]() {
                memcpy(leftPageFrameBufferNext, leftPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
                memcpy(rightPageFrameBufferNext, rightPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
                memcpy(leftPageFrameBuffer, leftPageFrameBufferPrevious, EPD_WIDTH * EPD_HEIGHT / 8);
                memcpy(rightPageFrameBuffer, rightPageFrameBufferPrevious, EPD_WIDTH * EPD_HEIGHT / 8);
                if(book->currentPage>=2)
                {
                    book->currentPage -= 2;
                    renderPages(leftPageFrameBufferPrevious, rightPageFrameBufferPrevious,0);
                    book->currentPage += 2;
                }
            }
        );
        book->currentPageElementIndex = pageElementIndex[1]; //store what chapter and html element the current page is at
        book->currentPageChapterIndex = pageChapterIndex[1];
        ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(pageImagePresent[0]),
         static_cast<int>(pageImagePresent[1]),
         static_cast<int>(pageImagePresent[2]));
    }
    else
    {
    }

    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
}

void SimpleReader::indexPages(void)
{
    book->chapterPageCounts.clear();
    book->cachedImages.clear();
    book->totalPageCount = 0;
    book->chapterCount = this->epub->get_spine_items_count();
    float percent = 0;
    float prevPercent = 0;
    for(int i=0;i<epub->get_spine_items_count();i++)
    {
        percent += 1/float(epub->get_spine_items_count());
        if(percent>=prevPercent+0.1) 
        {
            prevPercent += 0.1;
            //Device::getInstance().notificationHandler->drawIndexingNotification(book->title,int(100*percent));
        }
        std::string currentChapterPath = epub->get_spine_item(i);
        char *html = reinterpret_cast<char *>(this->epub->get_item_contents(currentChapterPath));
        if(html)
        {
            HtmlParser *parser = nullptr;
            ESP_LOGI(TAG, "render Chapter: %s", currentChapterPath.c_str());
            parser = new HtmlParser(html, strlen(html), currentChapterPath,this->renderer,-2,this->epub,leftPageFrameBuffer,rightPageFrameBuffer);
            parser->indexingMode = true;
            parser->currentChapterIndex = i;
            parser->currentChapterStartPageIndex = book->totalPageCount;
            parser->bookMarks = book->bookMarks;
            parser->cachedImages = book->cachedImages;
            Book::BookMark currentPageData = Book::BookMark(book->currentPage,book->currentPageChapterIndex,book->currentPageElementIndex);
            parser->bookMarks.push_back(currentPageData);
            ESP_LOGI(TAG, "requested element: %d", book->currentPageElementIndex);
            parser->parse();
            if(book->currentPageChapterIndex==i) 
            {
                book->currentPage = parser->bookMarks[parser->bookMarks.size()-1].pageIndex;
                if(book->currentPage%2) book->currentPage-=1;
            }
            parser->bookMarks.pop_back();
            book->bookMarks = parser->bookMarks;
            book->cachedImages = parser->cachedImages;
            int pageCount = (parser->pageReturn+1);
            if(pageCount%2) pageCount+=1;
            book->chapterPageCounts.push_back(pageCount);
           book->totalPageCount += pageCount;
            free(html);
            delete parser;
        } 
    }
}





void SimpleReader::leftPageAction()
{
        //Device::buzz();
        prevPage();
        //Device::getInstance().bookHandler->saveBook(book);
}

void SimpleReader::rightPageAction()
{
        nextPage();
        //Device::getInstance().bookHandler->saveBook(book);
}


void SimpleReader::middleButtonAction()
{

}
