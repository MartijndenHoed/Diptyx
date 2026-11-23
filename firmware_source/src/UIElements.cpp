#include "UIElements.h"
#include "device.h"

void MenuElement::renderElement()
{
    renderer->clearScreenBuffer();

    // Draw return arrow (back button) if not main menu
    if (elementName != "Diptyx E-reader") {
        renderer->drawCharacter(45, 0, 8617, false, false, 3, true); 
    }

    // Draw title centered
    renderer->drawString(
        (EPD_HEIGHT - elementName.length() * GLYPH_WIDTH) / 2,
        EPD_WIDTH - GLYPH_HEIGHT * 3,
        elementName,
        2, true, false, true
    );

    int displayElementStartindex = 0;
    int displayElementEndIndex = 0;
    int currentDisplayY = 0;
    bool selectedChildFound = false;
    bool endOfSelectedPageFound = false;
    bool lastPageSelected = false;
    int i=0;
    if(selectedChildIndex<0) selectedChildIndex=0;
    if(children.size()==0) return;

    while(!endOfSelectedPageFound) //first we check on which page the selected element is
    {
        currentDisplayY += children[i]->getElementHeight() + 1;
        
        if(i>=children.size()-1)
        {
            lastPageSelected = true;
            displayElementEndIndex = i+1;
            break;
        }

        if(i==selectedChildIndex)
        {
            selectedChildFound = true;
        }

        if(currentDisplayY > maxMenuheight )
        {
            if(selectedChildFound)
            {
                endOfSelectedPageFound = true;
                displayElementEndIndex = i+1;
            }
            else
            {
                displayElementStartindex = i+1;
                currentDisplayY = 0;
            }
        }
        i++;
    }

    if (displayElementStartindex > 0) {
        renderer->drawUpArrow(EPD_HEIGHT/2,EPD_WIDTH-3*GLYPH_HEIGHT,0,12,2,true);
        currentDisplayY += 12;
    }

    if (displayElementEndIndex < children.size()) {
        renderer->drawDownArrow(EPD_HEIGHT/2,EPD_WIDTH - (5*GLYPH_HEIGHT + currentDisplayY),0,12,2,true);
    }


    for(i = displayElementEndIndex-1; i >= displayElementStartindex;  i--) //and here we render the current page, from the bottom up
    {
        int y = EPD_WIDTH - (3*GLYPH_HEIGHT + currentDisplayY);
        children[i]->renderIcon(y,selectedChildIndex == i);
        currentDisplayY -= children[i]->getElementHeight() + 1;
    }

    firstElementOnPage = (selectedChildIndex==displayElementStartindex);
    lastElementOnPage = (selectedChildIndex==displayElementEndIndex-1);


    // const int maxVisible = MaxElementsPerPage; 
    // int currentPage = selectedChildIndex / maxVisible;
    // int startIndex = currentPage * maxVisible;
    // int endIndex = std::min(startIndex + maxVisible, (int)children.size());
    // int lowestY = 0;

    // // Draw visible children
    // for (int i = endIndex-1; i >= startIndex;  i--) {
    //     int localIndex = i - startIndex; // position within page
    //     int y = EPD_WIDTH - ((localIndex + 1.5) * 5 * GLYPH_HEIGHT);
    //     if(i==endIndex-1) lowestY = y;
    //     if(startIndex>0) y -= 2 * GLYPH_HEIGHT;

    //     children[i]->renderIcon(y,selectedChildIndex == i);

    //     // Draw arrow for currently selected item
    // }

    // Show up arrow if there are items above
    // if (startIndex > 0) {
    //     renderer->drawCharacter(
    //         EPD_HEIGHT / 2- GLYPH_WIDTH,       // x pos
    //         EPD_WIDTH - GLYPH_HEIGHT * 5, // near top
    //         8593, // ↑
    //         true, false, 2, true
    //     );
    // }

    // Show down arrow if there are items below
    // if (endIndex < children.size()) {
    //     int yOffset = 0;
    //     if(startIndex > 0) yOffset = 2*GLYPH_HEIGHT;
    //     renderer->drawCharacter(
    //         EPD_HEIGHT / 2 - GLYPH_WIDTH,       // x pos
    //         lowestY-3*GLYPH_HEIGHT - yOffset,         // near bottom
    //         8595, // ↓
    //         true, false, 2, true
    //     );
    // }

    
}

void UIElement::renderIcon(int y, bool highlight)
{
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,elementExtraDescription,false,highlight);
    if (highlight) {
        //renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
        renderer->drawRightArrow(EPD_HEIGHT - 30, y + 2*GLYPH_HEIGHT, 0, 12, 2, false);
    }
}

void MenuElement::renderIcon(int y, bool highlight)
{
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,std::string(""),false,highlight);
    if (highlight) {
        //renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
        renderer->drawRightArrow(EPD_HEIGHT - 30, y + 2*GLYPH_HEIGHT, 0, 12, 2, false);
    }
}

void BookElement::renderIcon(int y, bool highlight)
{
    if(!Device::getInstance().deviceSettings.showPagePercentage) this->elementDescription = (book->totalPageCount > 0 ? std::to_string((100*(book->currentPage))/(book->totalPageCount-2)) + "%" : "?%");
    else this->elementDescription = std::to_string(book->currentPage+1) + "/" +
                    (book->totalPageCount > 0 ? std::to_string(book->totalPageCount) : "?");
    this->elementExtraDescription = book->favorite?std::string("❤"):std::string("");
    renderer->drawTextBox(
        y,elementName.substr(0, 52),true,elementDescription,false,elementExtraDescription,false,highlight);
    if (highlight) {
        //renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
        renderer->drawRightArrow(EPD_HEIGHT - 30, y + 2*GLYPH_HEIGHT, 0, 12, 2, false);
    }
}

void RecentBookElement::renderIcon(int y, bool highlight)
{
    ESP_LOGI("debug", "Recent Book: %s",Device::getInstance().recentBookPath.c_str());
    this->book = Device::getInstance().bookHandler->findBookByPath(Device::getInstance().recentBookPath);
    if(book!=nullptr)
    {
        this->elementName = "Recently opened";
        this->elementDescription = book->title.substr(0, 52);
        if(!Device::getInstance().deviceSettings.showPagePercentage) this->elementExtraDescription = (book->totalPageCount > 0 ? std::to_string((100*(book->currentPage))/(book->totalPageCount-2)) + "%" : "?%");
        else this->elementExtraDescription = std::to_string(book->currentPage+1) + "/" +
                    (book->totalPageCount > 0 ? std::to_string(book->totalPageCount) : "?"); 
    }
    else
    {
        this->elementName = "No recently opened books";
        this->elementDescription = "";
        this->elementExtraDescription = "";
    }
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,elementExtraDescription,false,highlight);
    if (highlight && book!=nullptr) {
        //renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
        renderer->drawRightArrow(EPD_HEIGHT - 30, y + 2*GLYPH_HEIGHT, 0, 12, 2, false);
    }
}


void ValueElement::renderIcon(int y, bool highlight)
{
    renderer->drawTextBox(y,elementName,true,elementDescription,false,std::string(""),false,highlight);
    
    std::string valueString = this->valueDescriptions[selectedValueIndex];
    if(selected)
    {
        if(selectedValueIndex > 0 || infinitescrolling==true) valueString = "< " + valueString;
        if(selectedValueIndex<values.size()-1  || infinitescrolling==true) valueString = valueString + " >";
    }
    int stringWidth = (valueString.length()+1) * GLYPH_WIDTH/2;
    int textStartX = EPD_HEIGHT - 24 - stringWidth;
    if(!selected) renderer->drawString(textStartX, y + 1.5 * GLYPH_HEIGHT,valueString, 1,true, false, !highlight);
    else
    {
        renderer->drawRectangle(textStartX,y + 1.5 * GLYPH_HEIGHT,stringWidth,GLYPH_HEIGHT,false);
        renderer->drawString(textStartX, y + 1.5 * GLYPH_HEIGHT,valueString, 1,true, false, true);
    }
    drawValueYPos = y + 1.5 * GLYPH_HEIGHT;
    drawValueXPos = EPD_HEIGHT - 24 - (maxStringWidth+1)*GLYPH_WIDTH/2;
}

void ValueElement::writeValue()
{
    if(!this->values.empty()) *(this->valueAdress) = this->values[this->selectedValueIndex];
}

void ValueElement::ReadValue()
{
    for(int i=0;i<values.size();i++)
    {
        if(*(this->valueAdress) == this->values[i]) this->selectedValueIndex = i;
    }
}




void DropdownElement::renderIcon(int y, int centerX ,bool highlight)
{
    //renderer->drawTextBox(y, elementName, true, elementDescription, false, std::string(""), false, highlight);
    int startX = centerX;

    std::string valueString = valueDescriptions[selectedValueIndex];

    renderer->drawRectangle(startX - GLYPH_WIDTH/2, y,  (elementName.length() + 1 + 5 + valueString.length() + 3) * GLYPH_WIDTH/2,GLYPH_HEIGHT,!(!highlight || selected));
    renderer->drawString(startX, y, elementName + ":", 1, true, false, !highlight || selected);

    
    int maxLength = 0;
    for(auto value : valueDescriptions)
    {
        if(value.length() > maxLength) maxLength = value.length();
    }

    int textStartX = startX + GLYPH_WIDTH/2 * (elementName.length() + maxLength/2+3);
    if (!selected)
    {
        int centeringOffset = valueString.length() * GLYPH_WIDTH/4;
        renderer->drawString(textStartX-centeringOffset, y, valueString + " ▼", 1, true, false, !highlight);
    }


    else
    {
        int boxHeight = GLYPH_HEIGHT * (values.size());
        int boxWidth = (maxLength+2)* GLYPH_WIDTH/2;

        renderer->drawRectangle(textStartX - 0.5*boxWidth, y + GLYPH_HEIGHT - boxHeight-2,boxWidth+4,boxHeight+4,false);
        renderer->drawPaddedBox(textStartX - 0.5*boxWidth, y + GLYPH_HEIGHT - boxHeight-2,boxWidth+4,boxHeight+4,2,true);
        for(int i=0;i<valueDescriptions.size();i++)
        {
            int centeringOffset = valueDescriptions[i].length() * GLYPH_WIDTH/4;
            if(i!=selectedValueIndex) renderer->drawString(textStartX-centeringOffset, y - GLYPH_HEIGHT*i, valueDescriptions[i], 1, true, false, true);
            else {
                renderer->drawRectangle(textStartX - 0.5*boxWidth+2, y - GLYPH_HEIGHT*i, boxWidth, GLYPH_HEIGHT, true);
                renderer->drawString(textStartX-centeringOffset, y - GLYPH_HEIGHT*i, valueDescriptions[i], 1, true, false, false);
            }
        }
    }
}

void DropdownElement::writeValue()
{
    if(!this->values.empty()) *(this->valueAdress) = this->values[this->selectedValueIndex];
}

void DropdownElement::ReadValue()
{
    for(int i=0;i<values.size();i++)
    {
        if(*(this->valueAdress) == this->values[i]) this->selectedValueIndex = i;
    }
}

void FavoritesFolderElement::initChildren() {
        children.clear();

        int favoriteCount = 0;

        for (auto book : Device::getInstance().bookHandler->bookList) {
            if (book != nullptr && book->favorite) {
                auto bookElement = std::make_shared<BookElement>(renderer, book);
                addChild(bookElement);
                favoriteCount++;
            }
        }

        elementDescription = "Books: " + std::to_string(favoriteCount);
        elementName = "Favorite Books";
        if(selectedChildIndex>children.size()-1) selectedChildIndex = children.size()-1;
        if(children.size()==0) selectedChildIndex==0;
}

void HorizontalMenuElement::renderIcon(int y, bool highlight)
{
    if(children.size() == 0) return;
    int padding = 100;
    int horizontalSpacing;
    int startX;
    if(children.size() == 1)
    {
        horizontalSpacing = 0;
        startX = padding + (EPD_HEIGHT-2*padding)/2;
    }
    else {
        horizontalSpacing = (EPD_HEIGHT-2*padding) / (children.size()-1);
        startX = padding;
    }

    // Draw visible children
    for (int i = 0; i<children.size();i++) {
        bool elementHighlight = false;
        if(highlight && i==selectedChildIndex) elementHighlight = true;
        children[i]->renderIcon(y,startX + horizontalSpacing*i - children[i]->getElementWidth()/2,elementHighlight);
    }


    
}

void UIHorizontalElement::renderIcon(int y, int centerX ,bool highlight)
{
    int startX = centerX;

    renderer->drawRectangle(startX - GLYPH_WIDTH/2, y,  (elementName.length() + 2) * GLYPH_WIDTH/2,GLYPH_HEIGHT,highlight);
    renderer->drawString(startX, y, elementName, 1, true, false, !highlight);
}