#pragma once
#include <string>
#include <unordered_map>
#include "miniCSS.h"

enum TextAlign { LEFT_ALIGN, CENTERED, RIGHT_ALIGN };

class Style {
public:
    // Style properties
    int fontSize = 1;
    int bold = 0;
    int italic = 0;
    int indent = 0;
    int width = 0;
    int height = 0;
    bool widthSet = false;
    bool heightSet = false;
    int align = LEFT_ALIGN;
    

    // Constructor: optionally inherit from parent
    Style(const Style* parent = nullptr);

    // Apply a class name from cssCache to override properties
    void applyClass(const CSSCache& cssCache, const std::string& className);
    int translateDimension(const std::string& value, int parentValue);
private:
    // Translation helpers
    int translateFontSize(const std::string& fontSizeValue);
    int translateTextAlign(const std::string& textAlignValue);
    int translateFontWeight(const std::string& fontWeightValue);
    int translateItalic(const std::string& italicValue);
    int translateIndent(const std::string& indentValue);
    int translateHeight(const std::string& value, int parentValue);
    int translateWidth(const std::string& value, int parentValue);
};