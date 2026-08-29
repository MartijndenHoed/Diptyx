#include "bookMarkMenuHandler.h"
#include "esp_log.h"
#include "device.h"

BookMarkMenuHandler::BookMarkMenuHandler(Book *book, Renderer *renderer,Epub *epub,unsigned char* leftPageFrameBuffer,unsigned char* rightPageFrameBuffer)
{
    this->renderer = renderer;
    this->book = book;
    this->epub = epub;
    this->leftPageFrameBuffer = leftPageFrameBuffer;
    this->rightPageFrameBuffer = rightPageFrameBuffer;
    this->preservedFramebuffer = (unsigned char*)calloc(EPD_WIDTH * EPD_HEIGHT / 8,sizeof(unsigned char));
    memcpy(preservedFramebuffer, leftPageFrameBuffer, EPD_WIDTH * EPD_HEIGHT / 8);
}

BookMarkMenuHandler::~BookMarkMenuHandler()
{
    free(preservedFramebuffer);
}

void BookMarkMenuHandler::drawMenu()
{
    memcpy(leftPageFrameBuffer, preservedFramebuffer, EPD_WIDTH * EPD_HEIGHT / 8);

    renderer->framebuffer = leftPageFrameBuffer;
    int menuElementWidth = 150;
    int menuElementHeight = 50;
    renderer->drawSquare(0,0,EPD_HEIGHT,menuElementHeight+2,true);
    for(int i=0;i<menuElements.size();i++)
    {
        if(bookMarkOnPage && menuElements[i].text=="Add bookmark")  menuElements[i].text="Remove bookmark";
        if(!bookMarkOnPage && menuElements[i].text=="Remove bookmark")  menuElements[i].text="Add bookmark";

        if(!Device::getInstance().deviceSettings.showPagePercentage && menuElements[i].text=="Display pagecount")  menuElements[i].text="Display percentage";
        if(Device::getInstance().deviceSettings.showPagePercentage && menuElements[i].text=="Display percentage")  menuElements[i].text="Display pagecount";
        
        int pos = i - currentMenuElementIndex;

        int n = static_cast<int>(menuElements.size());
        int half = n / 2;

        if (pos > half) pos -= n;
        else if (pos < -half) pos += n;

        bool selected = (i==currentMenuElementIndex);// && (currentVerticalElementIndex==0);
        renderer->drawPaddedBox(EPD_HEIGHT/2-menuElementWidth/2+pos*menuElementWidth-2,0,menuElementWidth+4,menuElementHeight,4,!selected);
        std::string elementText = menuElements[i].text;
        int stringLength = elementText.length();
        renderer->drawString(EPD_HEIGHT/2-GLYPH_WIDTH*stringLength/4 + pos*menuElementWidth,menuElementHeight/2-GLYPH_HEIGHT/2,elementText,1,true,false,!selected);
    }

    int subElementHeight = 0;
    int scrollOffset = 0;
    if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoChapter || menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark)
    {
        const int MAX_VISIBLE_ITEMS = 20;

        int chapterCount = epub->get_toc_items_count();
        if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoChapter) maxVerticalElements = chapterCount;
        if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark) maxVerticalElements = book->bookMarks.size();

        // Calculate scroll offset based on current selection
        scrollOffset = 0;
        if (maxVerticalElements > MAX_VISIBLE_ITEMS) {
            // ensure the selected item is visible
            if (currentVerticalElementIndex > MAX_VISIBLE_ITEMS-1)
                scrollOffset = currentVerticalElementIndex - MAX_VISIBLE_ITEMS+1;
        }
        if(currentVerticalElementIndex==maxVerticalElements && scrollOffset!=0) scrollOffset -= 1;

        // Determine how many items to display in this window
        int visibleCount = std::min(maxVerticalElements - scrollOffset, MAX_VISIBLE_ITEMS);

        // Calculate total height
    subElementHeight = (visibleCount + 1) * GLYPH_HEIGHT;

        renderer->drawSquare(
            EPD_HEIGHT / 2 - menuElementWidth / 2 - 4,
            menuElementHeight,
            menuElementWidth + 8,
            subElementHeight - 2,
            true
        );
        renderer->drawPaddedBox(
            EPD_HEIGHT / 2 - menuElementWidth / 2 - 2,
            menuElementHeight - 4,
            menuElementWidth + 4,
            subElementHeight,
            4,
            true
        );

        // Draw "ARROW UP" if not at top
        if (scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements) {
             renderer->drawCharacter(
                EPD_HEIGHT / 2- GLYPH_WIDTH/4,
                menuElementHeight + (MAX_VISIBLE_ITEMS-1) * GLYPH_HEIGHT + 6,
                8593,
                false,
                false,
                1,
                true
            );
        }

        // Draw visible chapters
        for (int i = 0; i < visibleCount - (scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements); i++) {
            int chapterIndex = i + scrollOffset;
            if(scrollOffset!=0 && i==0) continue;
            //if(scrollOffset+MAX_VISIBLE_ITEMS < maxVerticalElements && i==visibleCount-1) continue;
            bool selected = (chapterIndex == currentVerticalElementIndex - 1);

            std::string itemString;
            if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoChapter) itemString = epub->get_toc_item(chapterIndex).title.substr(0, 18);
            if(menuElements[currentMenuElementIndex].ID==MenuElementID::GotoMark) itemString = "Page: " + std::to_string(book->bookMarks[i].pageIndex+1) + "-" + std::to_string(book->bookMarks[i].pageIndex+2);
            //ESP_LOGI("QuickMenu", "toc name: %s", itemString.c_str());

            int y = menuElementHeight + (i) * GLYPH_HEIGHT + 6;
            if (selected)
                renderer->drawSquare(
                    EPD_HEIGHT / 2 - itemString.length() * GLYPH_WIDTH / 4,
                    y,
                    itemString.length() * GLYPH_WIDTH / 2 + 1,
                    GLYPH_HEIGHT,
                    !selected
                );

            renderer->drawString(
                EPD_HEIGHT / 2 - itemString.length() * GLYPH_WIDTH / 4,
                y,
                itemString,
                1,
                false,
                false,
                !selected
            );
        }

        // Draw "ARROW DOWN" if not at end
        if (scrollOffset!=0) {
            renderer->drawCharacter(
                EPD_HEIGHT / 2- GLYPH_WIDTH/4,
                menuElementHeight + 6,
                8595,
                false,
                false,
                1,
                true
            );
        }
    }



    if(currentVerticalElementIndex==0 || currentVerticalElementIndex==currentTocIndex+1) renderer->epd.DisplayPictureBoth(leftPageFrameBuffer,rightPageFrameBuffer);
    else 
    {
        if(scrollOffset==0) renderer->epd.partialUpdatesRemaining[true] = 2;
        renderer->epd.DisplayPicturePartial(true, leftPageFrameBuffer,
                                EPD_HEIGHT / 2 - menuElementWidth / 2 - 4, menuElementHeight,
                                EPD_HEIGHT / 2 + menuElementWidth / 2 + 4, menuElementHeight+subElementHeight - 2);
    }
}
