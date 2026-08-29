#include "contentParser.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>
#include <algorithm>   
#include <device.h>

#define SpaceLookAheadMax 20


static const char *TAG = "contentParser";

ContentParser::ContentParser(Renderer* renderer)
{
    this->renderer = renderer;
    this->currentLine = 0;
    this->currentTextBlockIndex = 0;
}

ContentParser::~ContentParser()
{
}

void ContentParser::finishLine(bool draw, int align)
{
    if(draw) 
    {
        int yPos = EPD_WIDTH-(this->currentLine+1)*fontHeight;
        if(align==LEFT_ALIGN) this->renderer->drawString((GLYPH_WIDTH/2)*(1+Device::getInstance().renderSettings.marginsHorizontal),yPos,this->lineBuffer,fontSize,boldMask,italicsMask,true,false);
        if(align==RIGHT_ALIGN) this->renderer->drawString(EPD_HEIGHT-(Device::getInstance().renderSettings.marginsHorizontal * GLYPH_WIDTH/2)-(lineBufferWidth)*fontSize,yPos,this->lineBuffer,fontSize,boldMask,italicsMask,true,false);
        if(align==CENTERED) this->renderer->drawString((EPD_HEIGHT-lineBufferWidth*fontSize)/2,yPos,this->lineBuffer,fontSize,boldMask,italicsMask,true,false);
    }
    this->currentLine++;
    lineBuffer.clear();
    boldMask.clear();
    italicsMask.clear();
    lineBuffer.reserve(100);
    boldMask.reserve(100);
    italicsMask.reserve(100);
    lineBufferWidth=0;
    this->fontSize = 1;
}

void ContentParser::finishPage(bool draw)
{
    currentLine = 0;
    textOverflowBuffer.clear(); //should be cleared already, but why not
}

void ContentParser::parseImage(Image& image, Style& style,bool draw)
{
    fontHeight = Device::getInstance().renderer->fontHandler.currentFont.lineHeight + Device::getInstance().renderSettings.lineSpacing;
    if(!image.cached) image.prepare();
    ESP_LOGI(TAG, "Image height: %d",image.imageHeight);
    ESP_LOGI(TAG, "Image width: %d",image.imageWidth);
    if(style.widthSet && style.heightSet) 
    {
        style.width = std::min(style.width,EPD_HEIGHT);
        style.height = std::min(style.height,EPD_WIDTH);
    }
    else if(style.widthSet && !style.heightSet) 
    {
        style.width = std::min(style.width,EPD_HEIGHT);
        int newHeight = static_cast<int>((static_cast<float>(image.imageHeight)/static_cast<float>(image.imageWidth)) * static_cast<float>(style.width));
        style.height = std::min(newHeight,EPD_WIDTH);
    }
    else if(!style.widthSet && style.heightSet) 
    {
        style.height = std::min(style.height,EPD_WIDTH);
        int newWidth = static_cast<int>((static_cast<float>(image.imageWidth)/static_cast<float>(image.imageHeight)) * static_cast<float>(style.height));
        style.width = std::min(newWidth,EPD_HEIGHT);
    }
    else if(!style.widthSet && !style.heightSet)
    {
        style.height = std::min(image.imageHeight,EPD_WIDTH);
        style.width = std::min(image.imageWidth,EPD_HEIGHT);
    }

    if(style.height>EPD_WIDTH || style.width>EPD_HEIGHT)
    {
        style.width = std::min(style.width,EPD_HEIGHT);
        style.height = std::min(style.height,EPD_WIDTH);
        //image.scaleImage(newWidth,newHeight);
    }
    if(draw) 
    {
        image.decodeAndScale(style.width,style.height);
        image.floydSteinbergDither();
    }
    else 
    {
        image.imageWidth = style.width;
        image.imageHeight = style.height;
    }

    int w = image.imageWidth;
    int h = image.imageHeight;
    int x=0;
    if(style.align==RIGHT_ALIGN) x =  EPD_HEIGHT - w;
    if(style.align==CENTERED) x = EPD_HEIGHT/2 - w/2;
    if(style.align==LEFT_ALIGN) x=0;

    ESP_LOGI(TAG, "Final image height: %d",image.imageHeight);
    ESP_LOGI(TAG, "Final image width: %d",image.imageWidth);
    
    int y= (currentLine*fontHeight);
    currentLine += h/fontHeight+1;
    if(currentLine>maxLines+2 && y!=0 )
    {
        this->imageOverflowBuffer = &image;
        ESP_LOGI(TAG, "Image flows over, dumping it to the next page..",image.imageWidth);
        return;
    }
    if(draw) 
    {
        renderer->drawImage(image,x,y);
    }
}

void ContentParser::parseOverflowedImage(Style style,bool draw)
{
    Image& image = *(this->imageOverflowBuffer);
    int w = image.imageWidth;
    int h = image.imageHeight;
    int x=0;
    if(style.align==RIGHT_ALIGN) x =  EPD_HEIGHT - w;
    if(style.align==CENTERED) x = EPD_HEIGHT/2 - w/2;
    if(style.align==LEFT_ALIGN) x=0;

    int y= (currentLine*fontHeight);
    currentLine += h/fontHeight+1;
    if(draw) 
    {
        image.prepare();
        image.decodeAndScale(style.width,style.height);
        image.floydSteinbergDither();
        renderer->drawImage(image,x,y);
    }
    this->imageOverflowBuffer= nullptr;
}

void ContentParser::parseTextBlock(const std::vector<int> textBlock, bool newLine, Style style, bool draw)
{
    fontHeight = Device::getInstance().renderer->fontHandler.currentFont.lineHeight + Device::getInstance().renderSettings.lineSpacing;
    maxLines = (EPD_WIDTH/fontHeight)-Device::getInstance().renderSettings.marginsVertical-1;
    bool spaceFound = false; 
    bool bold = style.bold; 
    bool italics = style.italic; 
    int align = style.align; 
    if(this->currentLine==0) this->currentLine=Device::getInstance().renderSettings.marginsVertical;
    //if(newLine) this->currentLine+=this->fontSize-1;
    if(this->currentLine<0) this->currentLine=0; 
    int textBlockIndex = 0;
    int nextSpaceDistance = 0;
    int nextSpaceIndex = 0;
    int textBlockSize = textBlock.size();
    if(this->lineBuffer.size()==0) 
    {
        for(int i=0;i<2*style.indent;i++) 
        {
            lineBuffer.push_back(32);
            boldMask.push_back(0);
            italicsMask.push_back(0);  
            lineBufferWidth+= Device::getInstance().renderer->fontHandler.getFontCharWidth(32)  + bold;
        }
    }
    
    if(currentLine>=maxLines)
    {
        this->finishPage();
        flushToOVerflowBuffer(textBlock,textBlockIndex);
        return;
    }


    while(true)
    {
        this->currentLine += style.fontSize-this->fontSize;
        this->fontSize = style.fontSize; 
        if(!spaceFound)
        {
            nextSpaceDistance=0;
            nextSpaceIndex=0;
            while(nextSpaceIndex<SpaceLookAheadMax)
            {
                if(textBlockIndex+nextSpaceIndex>=textBlockSize || textBlock[textBlockIndex+nextSpaceIndex]==32) {
                    spaceFound=true;
                    break;
                }
                nextSpaceDistance+=Device::getInstance().renderer->fontHandler.getFontCharWidth(textBlock[textBlockIndex+nextSpaceIndex])  + bold; //find the distance to the next space
                nextSpaceIndex++;
            }
        }

        if((spaceFound && lineBufferWidth+nextSpaceDistance>(EPD_HEIGHT-(Device::getInstance().renderSettings.marginsHorizontal*2*GLYPH_WIDTH + 0.5*GLYPH_WIDTH))/this->fontSize) || (lineBufferWidth>(EPD_HEIGHT-(Device::getInstance().renderSettings.marginsHorizontal*2*GLYPH_WIDTH + 0.5*GLYPH_WIDTH))/this->fontSize))
        {
            finishLine(draw,align);
            this->currentLine+=this->fontSize-1;
            if(currentLine>=maxLines)
            {
                this->finishPage();
                flushToOVerflowBuffer(textBlock,textBlockIndex);
                return;
            }
        }

        if(textBlockIndex==textBlockSize)
        {
            return;
        }

        lineBuffer.push_back(textBlock[textBlockIndex]);
        boldMask.push_back(bold);
        italicsMask.push_back(italics);
        //lineBufferWidth+=renderer->fontHandler.getCharWidth(textBlock[textBlockIndex]);
        lineBufferWidth+=Device::getInstance().renderer->fontHandler.getFontCharWidth(textBlock[textBlockIndex]) + bold;
        nextSpaceDistance-=Device::getInstance().renderer->fontHandler.getFontCharWidth(textBlock[textBlockIndex]) + bold;
        if (--nextSpaceIndex <= 0) {
        spaceFound = false;
        }
        textBlockIndex++;


        if(textBlockIndex==textBlockSize)
        {
            return;
        }

    }

}

void ContentParser::flushToOVerflowBuffer(std::vector<int> textBlock,int textBlockIndex)
{
    for(int i=textBlockIndex;i<textBlock.size();i++)
    {
        this->textOverflowBuffer.push_back(textBlock[i]);
    }
}