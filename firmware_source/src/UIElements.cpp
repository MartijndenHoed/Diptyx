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

    const int maxVisible = MaxElementsPerPage; 
    int currentPage = selectedChildIndex / maxVisible;
    int startIndex = currentPage * maxVisible;
    int endIndex = std::min(startIndex + maxVisible, (int)children.size());
    int lowestY = 0;

    // Draw visible children
    for (int i = startIndex; i < endIndex; i++) {
        int localIndex = i - startIndex; // position within page
        int y = EPD_WIDTH - ((localIndex + 1.5) * 5 * GLYPH_HEIGHT);
        lowestY = y;
        if(startIndex>0) y -= 2 * GLYPH_HEIGHT;

        children[i]->renderIcon(y,selectedChildIndex == i);

        // Draw arrow for currently selected item
    }

    // Show up arrow if there are items above
    if (startIndex > 0) {
        renderer->drawCharacter(
            EPD_HEIGHT / 2- GLYPH_WIDTH,       // x pos
            EPD_WIDTH - GLYPH_HEIGHT * 5, // near top
            8593, // ↑
            true, false, 2, true
        );
    }

    // Show down arrow if there are items below
    if (endIndex < children.size()) {
        int yOffset = 0;
        if(startIndex > 0) yOffset = 2*GLYPH_HEIGHT;
        renderer->drawCharacter(
            EPD_HEIGHT / 2 - GLYPH_WIDTH,       // x pos
            lowestY-3*GLYPH_HEIGHT - yOffset,         // near bottom
            8595, // ↓
            true, false, 2, true
        );
    }

    
}

void UIElement::renderIcon(int y, bool highlight)
{
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,elementExtraDescription,false,highlight);
    if (highlight) {
        renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
    }
}

void MenuElement::renderIcon(int y, bool highlight)
{
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,std::string(""),false,highlight);
    if (highlight) {
        renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
    }
}

void BookElement::renderIcon(int y, bool highlight)
{
    if(!Device::getInstance().deviceSettings.showPagePercentage) this->elementDescription = (book->totalPageCount > 0 ? std::to_string((100*(book->currentPage))/(book->totalPageCount-2)) + "%" : "?%");
    else this->elementDescription = std::to_string(book->currentPage+1) + "/" +
                    (book->totalPageCount > 0 ? std::to_string(book->totalPageCount) : "?");
    this->elementExtraDescription = book->favorite?std::string("❤"):std::string("");
    renderer->drawTextBox(
        y,elementName,true,elementDescription,false,elementExtraDescription,false,highlight);
    if (highlight) {
        renderer->drawCharacter(EPD_HEIGHT - 60, y + 0.5 * GLYPH_HEIGHT,8594,true, false, 3, false);
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
        renderer->drawSquare(textStartX,y + 1.5 * GLYPH_HEIGHT,stringWidth,GLYPH_HEIGHT,true);
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


void MenuElement::enterElement()
{
    
}
