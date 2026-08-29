#include "notificationHandler.h"
#include "menuHandler.h"
#include "device.h"


NotificationHandler::NotificationHandler(Renderer *renderer)
{
    this->renderer = renderer;
}

void NotificationHandler::drawUSBQuery()
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();

    Device::usbState selectionState = Device::getInstance().usb_state;

    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Charge device or transfer files?");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    
    rectHeight = 2 * GLYPH_HEIGHT;
    rectWidth = EPD_HEIGHT/4;
    originX = 2*GLYPH_WIDTH;
    originY = originY-3 * GLYPH_HEIGHT;
    bool chargingSelected = (selectionState!=Device::usbState::Charging);
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,!chargingSelected);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,chargingSelected);
    displayString = std::string("Charge");
    renderer->drawString(originX + rectWidth/2 - displayString.length()*GLYPH_WIDTH/4, originY + 0.5*GLYPH_HEIGHT,displayString,1,true,false,chargingSelected);


    originX = EPD_HEIGHT - 2*GLYPH_WIDTH - rectWidth;
    bool transferSelected = (selectionState!=Device::usbState::FileTransfer);
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,!transferSelected);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,transferSelected);
    displayString = std::string("Transfer");
    renderer->drawString(originX + rectWidth/2 - displayString.length()*GLYPH_WIDTH/4, originY + 0.5*GLYPH_HEIGHT,displayString,1,true,false,transferSelected);
    
    if(transferSelected && chargingSelected) renderer->epd.forceRefresh();
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
    if(!transferSelected || !chargingSelected) renderer->epd.forceRefresh();
}

void NotificationHandler::drawManualFileTransfer(bool disconnect)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();


    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("You may now connect a USB type-C cable");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    
    rectHeight = 2 * GLYPH_HEIGHT;
    rectWidth = EPD_HEIGHT/4;
    originX = 2*GLYPH_WIDTH;
    originY = originY-3 * GLYPH_HEIGHT;

    originX = EPD_HEIGHT - 2*GLYPH_WIDTH - rectWidth;
    bool transferSelected = !disconnect;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,!transferSelected);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,transferSelected);
    displayString = std::string("Exit");
    renderer->drawString(originX + rectWidth/2 - displayString.length()*GLYPH_WIDTH/4, originY + 0.5*GLYPH_HEIGHT,displayString,1,true,false,transferSelected);
    
    if(transferSelected) renderer->epd.forceRefresh();
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}


void NotificationHandler::drawIndexingNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Loading book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPictureBoth(renderer->framebuffer,Device::getInstance().menuHandler->rightPageFrameBuffer);
}

void NotificationHandler::drawIndexingNotification(std::string bookTitle, int percent)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Indexing book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->drawProgressBar(originY-GLYPH_HEIGHT-4,51,percent);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawBookOpeningNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Opening book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}


void NotificationHandler::drawErrorNotification(std::string bookTitle)
{
    renderer->framebuffer = Device::getInstance().menuHandler->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = std::string("Error rendering book: ") + bookTitle;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Please inform the Diptyx development team");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawNotification(std::string notificationText)
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = notificationText;
    if (displayString.length() > 50) {
        displayString = displayString.substr(0, 50);
    }
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawSDcardErrorNotification()
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = "Error mounting SD card, shutting down :( ";
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Please refer to the Diptyx documentation");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}

void NotificationHandler::drawStorageAccessNotication()
{
    renderer->framebuffer = Device::getInstance().reader->leftPageFrameBuffer;
    renderer->clearScreenBuffer();
    int originY = 400;
    int originX = 2*GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    renderer->drawSquare(originX,originY,rectWidth,rectHeight,false);
    renderer->drawSquare(originX+padding,originY+padding,rectWidth-2*padding,rectHeight-2*padding,true);

    std::string displayString = "Debug storage access";
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 2.5*GLYPH_HEIGHT,displayString,1,true,false,true);
    displayString = std::string("Unplug USB cable to restart device");
    renderer->drawString(EPD_HEIGHT/2 - displayString.length()*GLYPH_WIDTH/4, originY + 1.5*GLYPH_HEIGHT,displayString,1,false,false,true);
    renderer->epd.DisplayPicture(true,renderer->framebuffer);
}