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
}


void SimpleReader::init(std::string bookName,Renderer* renderer)
{
    this->renderer = renderer;
    this->currentBookPath = bookName;
    this->currentChapter = 0;

    fileParser = createFileParser(std::string("/assets/") + this->currentBookPath);
    book = new Book(std::string("/assets/") + currentBookPath,
                this->currentBookPath,
                "title",
                "author",
                0);
    ESP_LOGI(TAG, "book path: %s", this->currentBookPath.c_str());
    Device::getInstance().notificationHandler->drawNotification("Opening " + this->currentBookPath);
    if (fileParser->load(book))
    {
        ESP_LOGI(TAG, "book title: %s", (book->title).c_str());
        //ESP_LOGI(TAG, "first chapter: %s", (epub->get_spine_item(0)).c_str());
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


int SimpleReader::renderPages(unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer,int pageCacheIndex)
{
    if(fileParser->renderPageToBuffers(book->currentPage,leftPageFrameBuffer,rightPageFrameBuffer,pageCacheIndex))
    {
        this->renderer->drawPageOverlay(leftPageFrameBuffer,book->currentPage+1,book->totalPageCount);
        this->renderer->drawPageOverlay(rightPageFrameBuffer,book->currentPage+2,book->totalPageCount);
        // if (Device::getInstance().deviceSettings.displayBattery) {
        //     int batteryLevel = Device::getInstance().getBatteryPercentage();;
        //     this->renderer->drawBattery(rightPageFrameBuffer,batteryLevel);
        // }
        if(book->currentPage==0) this->renderer->epd.forceRefresh();
        return true;
    }
    return false;
}

void SimpleReader::openPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(50000);
    renderPages(leftPageFrameBuffer, rightPageFrameBuffer,1);
    if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) renderer->epd.forceRefresh();
    imagePreviouslyPresent=fileParser->pageImagePresent[1];
    renderer->drawBattery(rightPageFrameBuffer,Device::getInstance().getBatteryPercentage());
    if(exitGuideEnabled)
    {
        std::string manualExitGuide = "To exit, finish the manual by proceeding to the last page";
        this->renderer->framebuffer = leftPageFrameBuffer;
        this->renderer->drawRectangle(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2, manualExitGuide.length()*GLYPH_WIDTH/2,GLYPH_HEIGHT,false);
        this->renderer->drawString(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2,manualExitGuide,1,false,false);
    }
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
    book->currentPageElementIndex = fileParser->pageElementIndex[1]; //store what chapter and html element the current page is at
    book->currentPageChapterIndex = fileParser->pageChapterIndex[1];
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
    
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
    if(exitGuideEnabled)
    {
        std::string manualExitGuide = "To exit, finish the manual by proceeding to the last page";
        this->renderer->framebuffer = leftPageFrameBufferNext;
        this->renderer->drawRectangle(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2, manualExitGuide.length()*GLYPH_WIDTH/2,GLYPH_HEIGHT,false);
        this->renderer->drawString(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2,manualExitGuide,1,false,false);
    }
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
    std::rotate(fileParser->pageChapterIndex.begin(), fileParser->pageChapterIndex.begin() + 1, fileParser->pageChapterIndex.end());
    std::rotate(fileParser->pageElementIndex.begin(), fileParser->pageElementIndex.begin() + 1, fileParser->pageElementIndex.end());
    std::rotate(fileParser->pageImagePresent.begin(), fileParser->pageImagePresent.begin() + 1, fileParser->pageImagePresent.end());
    if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) 
    {
        if( (100*abs(countBlackPixels(leftPageFrameBufferNext)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
        (100*abs(countBlackPixels(rightPageFrameBufferNext)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
        {
            renderer->epd.forceRefresh();
        }
    }
    imagePreviouslyPresent=fileParser->pageImagePresent[1];

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
    book->currentPageElementIndex = fileParser->pageElementIndex[1]; //store what chapter and html element the current page is at
    book->currentPageChapterIndex = fileParser->pageChapterIndex[1];
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));

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
        if(exitGuideEnabled)
        {
        std::string manualExitGuide = "To exit, finish the manual by proceeding to the last page";
        this->renderer->framebuffer = leftPageFrameBufferPrevious;
        this->renderer->drawRectangle(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2, manualExitGuide.length()*GLYPH_WIDTH/2,GLYPH_HEIGHT,false);
        this->renderer->drawString(EPD_HEIGHT/2 - manualExitGuide.length()*GLYPH_WIDTH/4,2,manualExitGuide,1,false,false);
        }   
        ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
        std::rotate(fileParser->pageChapterIndex.begin(), fileParser->pageChapterIndex.begin() + 2, fileParser->pageChapterIndex.end());
        std::rotate(fileParser->pageElementIndex.begin(), fileParser->pageElementIndex.begin() + 2, fileParser->pageElementIndex.end());
        std::rotate(fileParser->pageImagePresent.begin(), fileParser->pageImagePresent.begin() + 2, fileParser->pageImagePresent.end());
        if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) 
        {
            if( (100*abs(countBlackPixels(leftPageFrameBufferPrevious)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
            (100*abs(countBlackPixels(rightPageFrameBufferPrevious)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
            {
                renderer->epd.forceRefresh();
            }     
        }
        imagePreviouslyPresent=fileParser->pageImagePresent[1];

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
        book->currentPageElementIndex = fileParser->pageElementIndex[1]; //store what chapter and html element the current page is at
        book->currentPageChapterIndex = fileParser->pageChapterIndex[1];
        ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
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
    fileParser->indexChapters();
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
    if(exitGuideEnabled==false)
    {
        exitGuideEnabled = true;
        openPage();
    }
}
