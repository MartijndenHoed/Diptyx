#include "styleHandler.h"
#include <cstdlib>
#include <cmath>
#include <sstream>
#include <algorithm>
#include <device.h>

Style::Style(const Style* parent) {
    if(parent) {
        fontSize = parent->fontSize;
        bold = parent->bold;
        italic = parent->italic;
        indent = parent->indent;
        align = parent->align;
    }
    else
    {
        fontSize = Device::getInstance().renderSettings.fontSize;
        bold = Device::getInstance().renderSettings.fontBold;
    }
}

// ----------------------------
// Apply CSS class properties
void Style::applyClass(const CSSCache& cssCache, const std::string& className) {
    auto classIt = cssCache.find(className);
    if(classIt == cssCache.end()) return; // no CSS for this class

    const StyleMap& styles = classIt->second;

    if(styles.find("font-size") != styles.end())
        fontSize = translateFontSize(styles.at("font-size"));

    if(styles.find("font-weight") != styles.end())
        bold = translateFontWeight(styles.at("font-weight"));

    if(styles.find("font-style") != styles.end())
        italic = translateItalic(styles.at("font-style"));

    if(styles.find("text-align") != styles.end())
        align = translateTextAlign(styles.at("text-align"));

    if(styles.find("text-indent") != styles.end())
        indent = translateIndent(styles.at("text-indent"));;

    if(styles.find("width") != styles.end())
        width = translateWidth(styles.at("width"), this->width);

    if(styles.find("height") != styles.end())
        height = translateHeight(styles.at("height"), this->height);
}

// ----------------------------
// Translation helpers
int Style::translateFontSize(const std::string& fontSizeValue) {
    // 1. Handle text keywords
    if(fontSizeValue == "small") return Device::getInstance().renderSettings.fontSize;
    if(fontSizeValue == "medium") return Device::getInstance().renderSettings.fontSize;
    if(fontSizeValue == "large") return 2;

    // 2. Handle -em units
    size_t emPos = fontSizeValue.find("em");
    if(emPos != std::string::npos) {
        std::string numberPart = fontSizeValue.substr(0, emPos);
        char* endPtr = nullptr;
        double val = std::strtod(numberPart.c_str(), &endPtr);
        if(endPtr != numberPart.c_str()) {
            if(static_cast<int>(std::round(val))==0 || static_cast<int>(std::round(val))==1) return Device::getInstance().renderSettings.fontSize;
            else return static_cast<int>(std::round(val));
        }
        return Device::getInstance().renderSettings.fontSize; // fallback
    }
    return Device::getInstance().renderSettings.fontSize;
}

// ----------------------------
// Text alignment
int Style::translateTextAlign(const std::string& textAlignValue) {
    if(textAlignValue == "left") return LEFT_ALIGN;
    if(textAlignValue == "center") return CENTERED;
    if(textAlignValue == "right") return RIGHT_ALIGN;
    return LEFT_ALIGN;
}

// ----------------------------
// Font weight
int Style::translateFontWeight(const std::string& fontWeightValue) {
    if(fontWeightValue == "bold" || fontWeightValue == "bolder") return 1;
    return Device::getInstance().renderSettings.fontBold;
}

// ----------------------------
// Italic / oblique
int Style::translateItalic(const std::string& italicValue) {
    if(italicValue == "italic" || italicValue == "oblique") return 1;
    return 0;
}

// ----------------------------
// Text indent
int Style::translateIndent(const std::string& indentValue) {
    if (indentValue.empty()) return 0;

    // Handle -em units
    size_t emPos = indentValue.find("em");
    if (emPos != std::string::npos) {
        std::string numberPart = indentValue.substr(0, emPos);
        char* endPtr = nullptr;
        double val = std::strtod(numberPart.c_str(), &endPtr);
        if (endPtr != numberPart.c_str()) {
            return static_cast<int>(std::round(val));
        }
        return 0; // fallback
    }
    return 0;
}

int Style::translateDimension(const std::string& value, int parentValue) {
    if (value.empty()) return -1;

    //this->dimensionsSet = true;
    // Trim whitespace
    std::string valStr = value;
    while (!valStr.empty() && isspace(valStr.front())) valStr.erase(valStr.begin());
    while (!valStr.empty() && isspace(valStr.back())) valStr.pop_back();
    if(valStr.empty()) return -1;

    // Handle pixels
    size_t pxPos = valStr.find("px");
    if(pxPos != std::string::npos) {
        std::string numberPart = valStr.substr(0, pxPos);
        char* endPtr = nullptr;
        long val = std::strtol(numberPart.c_str(), &endPtr, 10);
        if(endPtr != numberPart.c_str() && *endPtr == '\0') return static_cast<int>(val);
        return -1; // fallback
    }

    // Handle percentage
    size_t percentPos = valStr.find("%");
    if(percentPos != std::string::npos) {
        std::string numberPart = valStr.substr(0, percentPos);
        char* endPtr = nullptr;
        double val = std::strtod(numberPart.c_str(), &endPtr);
        if(endPtr != numberPart.c_str()) {
            return static_cast<int>(std::round(val / 100.0 * parentValue));
        }
        return parentValue; // fallback
    }

    // Fallback numeric value
    char* endPtr = nullptr;
    long val = std::strtol(valStr.c_str(), &endPtr, 10);
    if(endPtr != valStr.c_str() && *endPtr == '\0') return static_cast<int>(val);

    return -1; // default
}

int Style::translateHeight(const std::string& value, int parentValue)
{
    int newHeight = translateDimension(value,parentValue);
    if(newHeight==-1) return parentValue;
    else
    {
        this->heightSet=true;
        return newHeight;
    }
}

int Style::translateWidth(const std::string& value, int parentValue)
{
    int newWidth = translateDimension(value,parentValue);
    if(newWidth==-1) return parentValue;
    else
    {
        this->widthSet=true;
        return newWidth;
    }
}