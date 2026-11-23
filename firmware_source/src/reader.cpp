#include "reader.h"
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

static const char *TAG = "reader";

Reader::Reader()
{
    leftPageFrameBuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    leftPageFrameBufferNext = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    leftPageFrameBufferPrevious = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    rightPageFrameBuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    rightPageFrameBufferNext = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    rightPageFrameBufferPrevious = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
}

Reader::~Reader()
{
    free(leftPageFrameBuffer);
    free(leftPageFrameBufferNext);
    free(leftPageFrameBufferPrevious);
    free(rightPageFrameBuffer);
    free(rightPageFrameBufferNext);
    free(rightPageFrameBufferPrevious);
}

inline uint8_t pop8(uint8_t v) {
    v = v - ((v >> 1) & 0x55);
    v = (v & 0x33) + ((v >> 2) & 0x33);
    return (((v + (v >> 4)) & 0x0F) * 0x01);
}

int countBlackPixels(unsigned char* framebuffer)
{
    size_t len = EPD_HEIGHT*EPD_WIDTH/8;
    int sum = 0;
    for (size_t i = 0; i < len; i++)
        sum += pop8(framebuffer[i]);

    return sum;
}

void Reader::init(Book *book,Renderer* renderer)
{
    this->renderer = renderer;
    this->book = book;
    this->currentBookPath = book->path;
    this->currentChapter = 0;

    //fileParser = createFileParser(std::string("/sdcard/") + this->currentBookPath);
    fileParser = createFileParser(this->currentBookPath);
    ESP_LOGI(TAG, "book path: %s", this->currentBookPath.c_str());
    if (fileParser->load(book))
    {
        ESP_LOGI(TAG, "book title: %s", (book->title).c_str());
        //ESP_LOGI(TAG, "first chapter: %s", (epub->get_spine_item(0)).c_str());
    }
}

void Reader::doWhileListening(std::function<void()> func)
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(100000);

    // Run the provided function
    func();

    // Then check latched button states
    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) { middleButtonAction(); return; }
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) { rightPageAction(); return; }
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON])  { leftPageAction(); return; }
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) { downButtonAction(); return; }
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON])   { upButtonAction(); return; }
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(100000);
}

bool Reader::renderPages(unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer,int pageCacheIndex)
{
    if(fileParser->renderPageToBuffers(book->currentPage,leftPageFrameBuffer,rightPageFrameBuffer,pageCacheIndex))
    {
        this->renderer->drawPageOverlay(leftPageFrameBuffer,book->currentPage+1,book->totalPageCount);
        this->renderer->drawPageOverlay(rightPageFrameBuffer,book->currentPage+2,book->totalPageCount);
        // if (Device::getInstance().deviceSettings.displayBattery) {
        //     int batteryLevel = Device::getInstance().getBatteryPercentage();;
        //     this->renderer->drawBattery(rightPageFrameBuffer,batteryLevel);
        // }
        if(checkBookMark()) renderer->drawBookMark(rightPageFrameBuffer,true);
        if(book->currentPage==0) this->renderer->epd.forceRefresh();
        return true;
    }
    return false;
}

void Reader::openPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(100000);
    renderPages(leftPageFrameBuffer, rightPageFrameBuffer,1);
    if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) renderer->epd.forceRefresh();
    imagePreviouslyPresent=fileParser->pageImagePresent[1];
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
    book->currentPageElementIndex = fileParser->pageElementIndex[1]; //store what chapter and html element the current page is at
    book->currentPageChapterIndex = fileParser->pageChapterIndex[1];
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
    
    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) {downButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON]) {upButtonAction(); return;}
}

void Reader::nextChapter()
{
    currentChapter = fileParser->getCurrentChapter(book->currentPage);
    if(currentChapter<book->chapterPageCounts.size()-1) 
    {
        currentChapter++;
        book->currentPage = fileParser->getCurrentPageAbsolute(currentChapter,0);
    }
    openPage();
}

void Reader::prevChapter()
{
    currentChapter = fileParser->getCurrentChapter(book->currentPage);
    if(currentChapter>0 && fileParser->getCurrentPageInChapter(book->currentPage)==0) currentChapter--;
    book->currentPage = fileParser->getCurrentPageAbsolute(currentChapter,0);
    openPage();
}

void Reader::nextPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(100000);
    if(book->currentPage >= book->totalPageCount-2)
    {
        middleButtonAction();
        return;
    }
    book->currentPage += 2;


    renderer->drawBattery(rightPageFrameBufferNext,Device::getInstance().getBatteryPercentage());
    ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
    std::rotate(fileParser->pageChapterIndex.begin(), fileParser->pageChapterIndex.begin() + 1, fileParser->pageChapterIndex.end());
    std::rotate(fileParser->pageElementIndex.begin(), fileParser->pageElementIndex.begin() + 1, fileParser->pageElementIndex.end());
    std::rotate(fileParser->pageImagePresent.begin(), fileParser->pageImagePresent.begin() + 1, fileParser->pageImagePresent.end());
    if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) 
    {
        if(Device::getInstance().deviceSettings.smartImageDetect)
        {
            if( (100*abs(countBlackPixels(leftPageFrameBufferNext)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
            (100*abs(countBlackPixels(rightPageFrameBufferNext)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
            {
                renderer->epd.forceRefresh();
            }
        }
        else   
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
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) {downButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON]) {upButtonAction(); return;}
}

void Reader::prevPage()
{
    Device::getInstance().clearButtonLatches();
    Device::getInstance().setLatchTimeOut(100000);
    if(book->currentPage>=2)
    {
        book->currentPage -= 2;
        renderer->drawBattery(rightPageFrameBufferPrevious,Device::getInstance().getBatteryPercentage());
        ESP_LOGI(TAG, "image present array: %d %d %d",
         static_cast<int>(fileParser->pageImagePresent[0]),
         static_cast<int>(fileParser->pageImagePresent[1]),
         static_cast<int>(fileParser->pageImagePresent[2]));
        std::rotate(fileParser->pageChapterIndex.begin(), fileParser->pageChapterIndex.begin() + 2, fileParser->pageChapterIndex.end());
        std::rotate(fileParser->pageElementIndex.begin(), fileParser->pageElementIndex.begin() + 2, fileParser->pageElementIndex.end());
        std::rotate(fileParser->pageImagePresent.begin(), fileParser->pageImagePresent.begin() + 2, fileParser->pageImagePresent.end());
        
        if((imagePreviouslyPresent!=fileParser->pageImagePresent[1]) || fileParser->pageImagePresent[1]) 
        {
            if(Device::getInstance().deviceSettings.smartImageDetect)
            {
                if( (100*abs(countBlackPixels(leftPageFrameBufferPrevious)-countBlackPixels(leftPageFrameBuffer)))/(648*480)>15 ||  
                (100*abs(countBlackPixels(rightPageFrameBufferPrevious)-countBlackPixels(rightPageFrameBuffer)))/(648*480)>15) //if the image is small and the overall page blackness level doesn't change too much, we don't need a full refresh
                {
                    renderer->epd.forceRefresh();
                }    
            }
            else
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
        middleButtonAction();
    }

    if(Device::getInstance().buttonLatchedStates[MIDDLE_BUTTON]) {middleButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_RIGHT_BUTTON]) {rightPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[PAGE_LEFT_BUTTON]) {leftPageAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_DOWN_BUTTON]) {downButtonAction(); return;}
    if(Device::getInstance().buttonLatchedStates[ARROW_UP_BUTTON]) {upButtonAction(); return;}
}

void Reader::indexPages(void)
{
    fileParser->indexChapters();
}

void Reader::addBookMark()
{
    for(int i=0;i<book->bookMarks.size();i++)
    {
        if(book->bookMarks[i].pageIndex==book->currentPage)
        {
            book->bookMarks.erase(book->bookMarks.begin() + i);
            return;
        }
    }

    book->bookMarks.push_back(Book::BookMark(book->currentPage,fileParser->pageChapterIndex[1],fileParser->pageElementIndex[1])); //store the data on the current bookmark
}

bool Reader::checkBookMark()
{
    for(int i=0;i<book->bookMarks.size();i++)
    {
        if(book->bookMarks[i].pageIndex==book->currentPage)
        {
            return true;
        }
    }
    return false;
}

int Reader::findCurrentBookMarkIndex()
{
    for(int i=0;i<book->bookMarks.size();i++)
    {
        if(book->bookMarks[i].pageIndex==book->currentPage)
        {
            return i;
        }
    }
    return -1;
}

void Reader::rightButtonAction() //the right and left buttons are no longer used
{
    // if(state==State::Reading)
    // {
    //     //Device::buzz();
    //     nextChapter();
    //     //Device::getInstance().bookHandler->saveBook(book);
    // }
    // //Device::buzz();
}

void Reader::leftButtonAction()
{
    // if(state==State::Reading)
    // {
    //     //Device::buzz();
    //     prevChapter();
    //     //Device::getInstance().bookHandler->saveBook(book);
    // }
    // //Device::buzz();
}

void Reader::upButtonAction()
{
    if(state==State::Reading)
    {
        //Device::buzz();
        state = State::BookMarkMenu;
        bookMarkMenuHandler = new BookMarkMenuHandler(book,renderer,fileParser.get(),leftPageFrameBuffer,rightPageFrameBuffer);
        bookMarkMenuHandler->bookMarkOnPage = checkBookMark();
        //bookMarkMenuHandler->currentTocIndex = epub->get_toc_index_for_spine_index(getCurrentChapter(book->currentPage));
        bookMarkMenuHandler->currentTocIndex = fileParser->getTocIndexFromChapterindex(fileParser->getCurrentChapter(book->currentPage));
        bookMarkMenuHandler->selectedBookMarkIndex = findCurrentBookMarkIndex();
        doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
    }
    else if(state==State::BookMarkMenu)
    {
                    ESP_LOGI(TAG, "element index %d", bookMarkMenuHandler->currentVerticalElementIndex);
            ESP_LOGI(TAG, "max elements %d",bookMarkMenuHandler->maxVerticalElements);
        if(bookMarkMenuHandler->currentVerticalElementIndex<bookMarkMenuHandler->maxVerticalElements)
        {
            //Device::buzz();

            bookMarkMenuHandler->currentVerticalElementIndex+=1;
            doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
        }
    }
}

void Reader::downButtonAction()
{
    if(state==State::Reading)
    {
        //Device::buzz();
        addBookMark();
        if(checkBookMark()) renderer->drawBookMark(rightPageFrameBuffer,true);
        else renderer->drawBookMark(rightPageFrameBuffer,false);
        this->renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
        //Device::getInstance().bookHandler->saveBook(book);
    }
    else if(state==State::BookMarkMenu)
    {
        //Device::buzz();
        // if(bookMarkMenuHandler->selectedBookMarkIndex<  static_cast<int>(book->bookMarks.size())-1) bookMarkMenuHandler->selectedBookMarkIndex++;
        // bookMarkMenuHandler->drawMenu();
        if(bookMarkMenuHandler->currentVerticalElementIndex<=0)
        {
            state = State::Reading;
            delete bookMarkMenuHandler;
            openPage();
        }
        else
        {
            bookMarkMenuHandler->currentVerticalElementIndex-=1;
            doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
        }

    }

}

void Reader::leftPageAction()
{
    if(state==State::Reading)
    {
        //Device::buzz();
        prevPage();
        //Device::getInstance().bookHandler->saveBook(book);
    }
    else if(state==State::BookMarkMenu)
    {
        //Device::buzz();
        bookMarkMenuHandler->currentMenuElementIndex-=1;
        if(bookMarkMenuHandler->currentMenuElementIndex<0) bookMarkMenuHandler->currentMenuElementIndex=bookMarkMenuHandler->menuElements.size()-1;
        if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::GotoChapter) bookMarkMenuHandler->currentVerticalElementIndex = bookMarkMenuHandler->currentTocIndex+1;
        else bookMarkMenuHandler->currentVerticalElementIndex = 0;
        doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
    }
}

void Reader::rightPageAction()
{
    if(state==State::Reading)
    {
        //Device::buzz();
        nextPage();
        //Device::getInstance().bookHandler->saveBook(book);
    }
    else if(state==State::BookMarkMenu)
    {
        //Device::buzz();
        bookMarkMenuHandler->currentMenuElementIndex+=1;
        if(bookMarkMenuHandler->currentMenuElementIndex>bookMarkMenuHandler->menuElements.size()-1) bookMarkMenuHandler->currentMenuElementIndex=0;
        if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::GotoChapter) bookMarkMenuHandler->currentVerticalElementIndex = bookMarkMenuHandler->currentTocIndex+1;
        else bookMarkMenuHandler->currentVerticalElementIndex = 0;
        doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
    }
}


void Reader::middleButtonAction()
{
    if(state==State::Reading)
    {
       // Device::buzz();
        Device::getInstance().state=Device::State::Menu;
        //Device::getInstance()->saveAppState();
        renderer->epd.forceRefresh();

        //std::static_pointer_cast<BookElement> (
        //std::static_pointer_cast<MenuElement>(Device::getInstance().menuHandler->currentElement)->children[
        //std::static_pointer_cast<MenuElement>(Device::getInstance().menuHandler->currentElement)->selectedChildIndex
        //])->elementDescription = std::to_string(book->currentPage+1) + "/" + std::to_string(book->totalPageCount); //update the pagecount of the book icon //actually book descriptions auto update now

        Device::getInstance().bookHandler->saveBook(book);
        Device::getInstance().menuHandler->drawMenu();
        Device::getInstance().saveAppState();
        //Device::buzz();
    }
    else if(state==State::BookMarkMenu)
    {
        // if(bookMarkMenuHandler->selectedBookMarkIndex!=-1) book->currentPage=book->bookMarks[bookMarkMenuHandler->selectedBookMarkIndex];
        // state = State::Reading;
        // delete bookMarkMenuHandler;
        // openPage();
        if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::Return)
        {
            //Device::buzz();
                state = State::Reading;
            delete bookMarkMenuHandler;
            renderer->epd.forceRefresh();
            openPage();
            //Device::getInstance().bookHandler->saveBook(book);
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::pageDisplay)
        {
            //Device::buzz();
            Device::getInstance().deviceSettings.showPagePercentage = !Device::getInstance().deviceSettings.showPagePercentage;
                state = State::Reading;
            delete bookMarkMenuHandler;
            renderer->epd.forceRefresh();
            Device::getInstance().saveSettings();
            openPage();
            //Device::getInstance().bookHandler->saveBook(book);
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::NextChapt)
        {
            //Device::buzz();
            state = State::Reading;
            delete bookMarkMenuHandler;
            renderer->epd.forceRefresh();
            nextChapter();
            //Device::getInstance().bookHandler->saveBook(book);
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::PrevChapt)
        {
            //Device::buzz();
            state = State::Reading;
            delete bookMarkMenuHandler;
            renderer->epd.forceRefresh();
            prevChapter();
            //Device::getInstance().bookHandler->saveBook(book);
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::AddMark)
        {
            //Device::buzz();
            addBookMark();
            bookMarkMenuHandler->bookMarkOnPage = !bookMarkMenuHandler->bookMarkOnPage;
            if(checkBookMark()) renderer->drawBookMark(rightPageFrameBuffer,true);
            else renderer->drawBookMark(rightPageFrameBuffer,false);
            doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
            this->renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
            //Device::getInstance().bookHandler->saveBook(book);
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::ToggleNightMode)
        {
            //Device::buzz();
            Device::getInstance().deviceSettings.nightMode = !Device::getInstance().deviceSettings.nightMode;
            //this->renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
            Device::getInstance().saveSettings();
            renderer->epd.forceRefresh();
            doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::ToggleSunlightMode)
        {
            //Device::buzz();
            Device::getInstance().deviceSettings.sunlightMode = !Device::getInstance().deviceSettings.sunlightMode;
            //this->renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
            Device::getInstance().saveSettings();
            renderer->epd.forceRefresh();
            renderer->drawBattery(rightPageFrameBuffer,Device::getInstance().getBatteryPercentage());
            doWhileListening([&]() {bookMarkMenuHandler->drawMenu();});
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::GotoMark)
        {
            if(bookMarkMenuHandler->currentVerticalElementIndex!=0)
            {
                //Device::buzz();
                book->currentPage=book->bookMarks[bookMarkMenuHandler->currentVerticalElementIndex-1].pageIndex;
                state = State::Reading;
                delete bookMarkMenuHandler;
                openPage();  
            }
        }
        else if(bookMarkMenuHandler->menuElements[bookMarkMenuHandler->currentMenuElementIndex].ID==BookMarkMenuHandler::MenuElementID::GotoChapter)
        {
            if(bookMarkMenuHandler->currentVerticalElementIndex!=0)
            {
                //Device::buzz();
                int newChapterIndex = fileParser->getChapterIndexFromTOCindex(bookMarkMenuHandler->currentVerticalElementIndex-1);
                book->currentPage = fileParser->getCurrentPageAbsolute(newChapterIndex,0);
                state = State::Reading;
                delete bookMarkMenuHandler;
                openPage();  
            }
        }
        
    }
}
