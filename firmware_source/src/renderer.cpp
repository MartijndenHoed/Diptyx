#include "renderer.h"
#include "esp_log.h"
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <cctype>
#include "device.h"



static const char *TAG = "renderer";


static const std::unordered_map<std::string, int> htmlEntities = {
    // Basic XML / HTML entities
    {"amp",    0x0026}, // &  Ampersand
    {"lt",     0x003C}, // <  Less than
    {"gt",     0x003E}, // >  Greater than
    {"quot",   0x0022}, // "  Double quotation mark
    {"apos",   0x0027}, // '  Apostrophe
    {"nbsp",   0x00A0}, //    Non-breaking space

    // Latin-1: punctuation and symbols
    {"iexcl",  0x00A1}, // ¡  Inverted exclamation mark
    {"cent",   0x00A2}, // ¢  Cent sign
    {"pound",  0x00A3}, // £  Pound sign
    {"curren", 0x00A4}, // ¤  Currency sign
    {"yen",    0x00A5}, // ¥  Yen sign
    {"brvbar", 0x00A6}, // ¦  Broken bar
    {"sect",   0x00A7}, // §  Section sign
    {"uml",    0x00A8}, // ¨  Diaeresis
    {"copy",   0x00A9}, // ©  Copyright sign
    {"ordf",   0x00AA}, // ª  Feminine ordinal indicator
    {"laquo",  0x00AB}, // «  Left double angle quotation mark
    {"not",    0x00AC}, // ¬  Not sign
    {"shy",    0x00AD}, //    Soft hyphen
    {"reg",    0x00AE}, // ®  Registered sign
    {"macr",   0x00AF}, // ¯  Macron
    {"deg",    0x00B0}, // °  Degree sign
    {"plusmn", 0x00B1}, // ±  Plus-minus sign
    {"sup2",   0x00B2}, // ²  Superscript two
    {"sup3",   0x00B3}, // ³  Superscript three
    {"acute",  0x00B4}, // ´  Acute accent
    {"micro",  0x00B5}, // µ  Micro sign
    {"para",   0x00B6}, // ¶  Pilcrow
    {"middot", 0x00B7}, // ·  Middle dot
    {"cedil",  0x00B8}, // ¸  Cedilla
    {"sup1",   0x00B9}, // ¹  Superscript one
    {"ordm",   0x00BA}, // º  Masculine ordinal indicator
    {"raquo",  0x00BB}, // »  Right double angle quotation mark
    {"frac14", 0x00BC}, // ¼  One quarter
    {"frac12", 0x00BD}, // ½  One half
    {"frac34", 0x00BE}, // ¾  Three quarters
    {"iquest", 0x00BF}, // ¿  Inverted question mark

    // Latin-1: accented letters
    {"Agrave", 0x00C0}, // À
    {"Aacute", 0x00C1}, // Á
    {"Acirc",  0x00C2}, // Â
    {"Atilde", 0x00C3}, // Ã
    {"Auml",   0x00C4}, // Ä
    {"Aring",  0x00C5}, // Å
    {"AElig",  0x00C6}, // Æ
    {"Ccedil", 0x00C7}, // Ç
    {"Egrave", 0x00C8}, // È
    {"Eacute", 0x00C9}, // É
    {"Ecirc",  0x00CA}, // Ê
    {"Euml",   0x00CB}, // Ë
    {"Igrave", 0x00CC}, // Ì
    {"Iacute", 0x00CD}, // Í
    {"Icirc",  0x00CE}, // Î
    {"Iuml",   0x00CF}, // Ï
    {"ETH",    0x00D0}, // Ð
    {"Ntilde", 0x00D1}, // Ñ
    {"Ograve", 0x00D2}, // Ò
    {"Oacute", 0x00D3}, // Ó
    {"Ocirc",  0x00D4}, // Ô
    {"Otilde", 0x00D5}, // Õ
    {"Ouml",   0x00D6}, // Ö
    {"times",  0x00D7}, // ×
    {"Oslash", 0x00D8}, // Ø
    {"Ugrave", 0x00D9}, // Ù
    {"Uacute", 0x00DA}, // Ú
    {"Ucirc",  0x00DB}, // Û
    {"Uuml",   0x00DC}, // Ü
    {"Yacute", 0x00DD}, // Ý
    {"THORN",  0x00DE}, // Þ
    {"szlig",  0x00DF}, // ß

    {"agrave", 0x00E0}, // à
    {"aacute", 0x00E1}, // á
    {"acirc",  0x00E2}, // â
    {"atilde", 0x00E3}, // ã
    {"auml",   0x00E4}, // ä
    {"aring",  0x00E5}, // å
    {"aelig",  0x00E6}, // æ
    {"ccedil", 0x00E7}, // ç
    {"egrave", 0x00E8}, // è
    {"eacute", 0x00E9}, // é
    {"ecirc",  0x00EA}, // ê
    {"euml",   0x00EB}, // ë
    {"igrave", 0x00EC}, // ì
    {"iacute", 0x00ED}, // í
    {"icirc",  0x00EE}, // î
    {"iuml",   0x00EF}, // ï
    {"eth",    0x00F0}, // ð
    {"ntilde", 0x00F1}, // ñ
    {"ograve", 0x00F2}, // ò
    {"oacute", 0x00F3}, // ó
    {"ocirc",  0x00F4}, // ô
    {"otilde", 0x00F5}, // õ
    {"ouml",   0x00F6}, // ö
    {"divide", 0x00F7}, // ÷
    {"oslash", 0x00F8}, // ø
    {"ugrave", 0x00F9}, // ù
    {"uacute", 0x00FA}, // ú
    {"ucirc",  0x00FB}, // û
    {"uuml",   0x00FC}, // ü
    {"yacute", 0x00FD}, // ý
    {"thorn",  0x00FE}, // þ
    {"yuml",   0x00FF}, // ÿ

    // Ligatures and special Latin letters
    {"OElig",  0x0152}, // Œ
    {"oelig",  0x0153}, // œ
    {"Scaron", 0x0160}, // Š
    {"scaron", 0x0161}, // š
    {"Yuml",   0x0178}, // Ÿ
    {"fnof",   0x0192}, // ƒ

    // Spacing and directional marks
    {"ensp",   0x2002}, // En space
    {"emsp",   0x2003}, // Em space
    {"thinsp", 0x2009}, // Thin space
    {"zwnj",   0x200C}, // Zero-width non-joiner
    {"zwj",    0x200D}, // Zero-width joiner
    {"lrm",    0x200E}, // Left-to-right mark
    {"rlm",    0x200F}, // Right-to-left mark

    // Typographic punctuation
    {"ndash",  0x2013}, // –  En dash
    {"mdash",  0x2014}, // —  Em dash
    {"lsquo",  0x2018}, // ‘  Left single quotation mark
    {"rsquo",  0x2019}, // ’  Right single quotation mark
    {"sbquo",  0x201A}, // ‚  Single low-9 quotation mark
    {"ldquo",  0x201C}, // “  Left double quotation mark
    {"rdquo",  0x201D}, // ”  Right double quotation mark
    {"bdquo",  0x201E}, // „  Double low-9 quotation mark
    {"dagger", 0x2020}, // †  Dagger
    {"Dagger", 0x2021}, // ‡  Double dagger
    {"bull",   0x2022}, // •  Bullet
    {"hellip", 0x2026}, // …  Horizontal ellipsis
    {"permil", 0x2030}, // ‰  Per mille
    {"prime",  0x2032}, // ′  Prime
    {"Prime",  0x2033}, // ″  Double prime
    {"lsaquo", 0x2039}, // ‹  Single left angle quotation mark
    {"rsaquo", 0x203A}, // ›  Single right angle quotation mark
    {"oline",  0x203E}, // ‾  Overline
    {"frasl",  0x2044}, // ⁄  Fraction slash

    // Currency
    {"euro",   0x20AC}, // €  Euro sign

    // Mathematical operators and symbols
    {"weierp", 0x2118}, // ℘  Weierstrass p
    {"image",  0x2111}, // ℑ  Imaginary part
    {"real",   0x211C}, // ℜ  Real part
    {"trade",  0x2122}, // ™  Trademark
    {"alefsym",0x2135}, // ℵ  Alef symbol
    {"larr",   0x2190}, // ←  Left arrow
    {"uarr",   0x2191}, // ↑  Up arrow
    {"rarr",   0x2192}, // →  Right arrow
    {"darr",   0x2193}, // ↓  Down arrow
    {"harr",   0x2194}, // ↔  Left-right arrow
    {"crarr",  0x21B5}, // ↵  Carriage return arrow
    {"lArr",   0x21D0}, // ⇐  Left double arrow
    {"uArr",   0x21D1}, // ⇑  Up double arrow
    {"rArr",   0x21D2}, // ⇒  Right double arrow
    {"dArr",   0x21D3}, // ⇓  Down double arrow
    {"hArr",   0x21D4}, // ⇔  Left-right double arrow
    {"forall", 0x2200}, // ∀  For all
    {"part",   0x2202}, // ∂  Partial differential
    {"exist",  0x2203}, // ∃  There exists
    {"empty",  0x2205}, // ∅  Empty set
    {"nabla",  0x2207}, // ∇  Nabla
    {"isin",   0x2208}, // ∈  Element of
    {"notin",  0x2209}, // ∉  Not an element of
    {"ni",     0x220B}, // ∋  Contains as member
    {"prod",   0x220F}, // ∏  Product
    {"sum",    0x2211}, // ∑  Summation
    {"minus",  0x2212}, // −  Minus sign
    {"lowast", 0x2217}, // ∗  Asterisk operator
    {"radic",  0x221A}, // √  Square root
    {"prop",   0x221D}, // ∝  Proportional to
    {"infin",  0x221E}, // ∞  Infinity
    {"ang",    0x2220}, // ∠  Angle
    {"and",    0x2227}, // ∧  Logical AND
    {"or",     0x2228}, // ∨  Logical OR
    {"cap",    0x2229}, // ∩  Intersection
    {"cup",    0x222A}, // ∪  Union
    {"int",    0x222B}, // ∫  Integral
    {"there4", 0x2234}, // ∴  Therefore
    {"sim",    0x223C}, // ∼  Tilde operator
    {"cong",   0x2245}, // ≅  Approximately equal
    {"asymp",  0x2248}, // ≈  Almost equal
    {"ne",     0x2260}, // ≠  Not equal
    {"equiv",  0x2261}, // ≡  Equivalent to
    {"le",     0x2264}, // ≤  Less than or equal
    {"ge",     0x2265}, // ≥  Greater than or equal
    {"sub",    0x2282}, // ⊂  Subset
    {"sup",    0x2283}, // ⊃  Superset
    {"nsub",   0x2284}, // ⊄  Not a subset
    {"sube",   0x2286}, // ⊆  Subset or equal
    {"supe",   0x2287}, // ⊇  Superset or equal
    {"oplus",  0x2295}, // ⊕  Circled plus
    {"otimes", 0x2297}, // ⊗  Circled times
    {"perp",   0x22A5}, // ⊥  Perpendicular
    {"sdot",   0x22C5}, // ⋅  Dot operator

    // Miscellaneous technical symbols
    {"lceil",  0x2308}, // ⌈  Left ceiling
    {"rceil",  0x2309}, // ⌉  Right ceiling
    {"lfloor", 0x230A}, // ⌊  Left floor
    {"rfloor", 0x230B}, // ⌋  Right floor
    {"lang",   0x2329}, // 〈  Left angle bracket
    {"rang",   0x232A}, // 〉  Right angle bracket

    // Geometric shapes and card suits
    {"loz",    0x25CA}, // ◊  Lozenge
    {"spades", 0x2660}, // ♠  Spades
    {"clubs",  0x2663}, // ♣  Clubs
    {"hearts", 0x2665}, // ♥  Hearts
    {"diams",  0x2666}, // ♦  Diamonds
};

int utf8ToCodePoints(const char* utf8, int* out, int maxOutLen) {
    int i = 0;  // Index in utf8 input
    int j = 0;  // Index in output array

    while (utf8[i] && j < maxOutLen) {
        uint8_t c = (uint8_t)utf8[i];
        int codepoint = 0;
        int bytes = 0;

        if ((c & 0x80) == 0x00) {
            // 1-byte ASCII
            codepoint = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8
            codepoint = (c & 0x1F) << 6;
            codepoint |= ((uint8_t)utf8[i+1] & 0x3F);
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8
            codepoint = (c & 0x0F) << 12;
            codepoint |= ((uint8_t)utf8[i+1] & 0x3F) << 6;
            codepoint |= ((uint8_t)utf8[i+2] & 0x3F);
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte UTF-8
            codepoint = (c & 0x07) << 18;
            codepoint |= ((uint8_t)utf8[i+1] & 0x3F) << 12;
            codepoint |= ((uint8_t)utf8[i+2] & 0x3F) << 6;
            codepoint |= ((uint8_t)utf8[i+3] & 0x3F);
            bytes = 4;
        } else {
            // Invalid byte, skip
            i++;
            continue;
        }

        out[j++] = codepoint;
        i += bytes;
    }

    return j;  // Number of codepoints written
}

std::vector<int> decodeHtmlEntities(const std::vector<int>& input) {
    std::vector<int> output;

    size_t i = 0;
    bool lastWasSpace = false;

    while (i < input.size()) {
        // Handle HTML entities
        if (input[i] == '&') {
            // Collect characters until ';' or non-ASCII
            std::string entity;
            size_t j = i + 1;
            while (j < input.size() && input[j] != ';' && input[j] < 128) {
                entity.push_back(static_cast<char>(input[j]));
                j++;
            }

            if (j < input.size() && input[j] == ';') {
                int codepoint = -1;

                if (!entity.empty() && entity[0] == '#') {
                    // Numeric entity
                    if (entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X')) {
                        codepoint = std::strtol(entity.c_str() + 2, nullptr, 16);
                    } else {
                        codepoint = std::strtol(entity.c_str() + 1, nullptr, 10);
                    }
                } else {
                    // Named entity
                    auto it = htmlEntities.find(entity);
                    if (it != htmlEntities.end()) {
                        codepoint = it->second;
                    }
                }

                if (codepoint != -1) {
                    // Collapse whitespace from entities (nbsp, etc.)
                    if (codepoint == ' ' || codepoint == '\t' || codepoint == '\n' ||
                        codepoint == '\r' || codepoint == 0x00A0) 
                    {
                        if (!lastWasSpace)
                            output.push_back(' ');
                        lastWasSpace = true;
                    } else {
                        output.push_back(codepoint);
                        lastWasSpace = false;
                    }

                    i = j + 1;
                    continue;
                }
            }
        }

        int cp = input[i];

        if(cp==0xFEFF) //zero-width space
        {
            i++;
            continue;
        }

        // Whitespace collapsers
        bool isSpaceChar =
            (cp == ' '  || cp == '\t' ||
             cp == '\n' || cp == '\r' ||
             cp == 0x00A0 || cp == 0x000A|| cp == 0x0009); 

        if (isSpaceChar) {
            if (!lastWasSpace)
                output.push_back(' ');
            lastWasSpace = true;
            i++;
            continue;
        }

        // Regular character
        output.push_back(cp);
        lastWasSpace = false;
        i++;
    }

    return output;
}



std::vector<int> utf8ToCodePoints(const std::string& utf8) {
    std::vector<int> codepoints;

    size_t i = 0;
    while (i < utf8.size()) {
        uint8_t c = static_cast<uint8_t>(utf8[i]);
        int codepoint = 0;
        int bytes = 0;

        if ((c & 0x80) == 0x00) {
            // 1-byte ASCII
            codepoint = c;
            bytes = 1;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte UTF-8
            codepoint = (c & 0x1F) << 6;
            codepoint |= (static_cast<uint8_t>(utf8[i+1]) & 0x3F);
            bytes = 2;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte UTF-8
            codepoint = (c & 0x0F) << 12;
            codepoint |= (static_cast<uint8_t>(utf8[i+1]) & 0x3F) << 6;
            codepoint |= (static_cast<uint8_t>(utf8[i+2]) & 0x3F);
            bytes = 3;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte UTF-8
            codepoint = (c & 0x07) << 18;
            codepoint |= (static_cast<uint8_t>(utf8[i+1]) & 0x3F) << 12;
            codepoint |= (static_cast<uint8_t>(utf8[i+2]) & 0x3F) << 6;
            codepoint |= (static_cast<uint8_t>(utf8[i+3]) & 0x3F);
            bytes = 4;
        } else {
            // Invalid UTF-8 byte, skip it
            i++;
            continue;
        }

        codepoints.push_back(codepoint);
        i += bytes;
    }

    return codepoints;
}

std::vector<int> ISO8859_1ToCodePoints(const std::string& input) {
    std::vector<int> codepoints;
    codepoints.reserve(input.size());

    // Windows-1252 mappings for bytes 0x80–0x9F.
    static constexpr int windows1252[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, // 80–83
        0x201E, 0x2026, 0x2020, 0x2021, // 84–87
        0x02C6, 0x2030, 0x0160, 0x2039, // 88–8B
        0x0152, 0x008D, 0x017D, 0x008F, // 8C–8F
        0x0090, 0x2018, 0x2019, 0x201C, // 90–93
        0x201D, 0x2022, 0x2013, 0x2014, // 94–97
        0x02DC, 0x2122, 0x0161, 0x203A, // 98–9B
        0x0153, 0x009D, 0x017E, 0x0178  // 9C–9F
    };

    for (unsigned char c : input) {
        if (c >= 0x80 && c <= 0x9F) {
            codepoints.push_back(windows1252[c - 0x80]);
        } else {
            // Other bytes map directly, as in ISO-8859-1.
            codepoints.push_back(static_cast<int>(c));
        }
    }

    return codepoints;
}

void Renderer::init(void)
{
    //framebuffer = (unsigned char*)malloc(EPD_WIDTH * EPD_HEIGHT / 8);
    //this->clearScreenBuffer();
    if (this->epd.Init() != 0) {
        ESP_LOGE(TAG, "e-Paper init failed");
        return;
    }

    ESP_LOGI(TAG, "e-Paper init");
}


void Renderer::renderChecker(void)
{
    for(long i=0;i<EPD_WIDTH/8;i++)
    {
        for(long j=0;j<EPD_HEIGHT;j++)
        {
            if(j%2==0) this->framebuffer[i*EPD_HEIGHT + j] = 0x00;
            else this->framebuffer[i*EPD_HEIGHT + j] = 0xFF;
        }
    }
}


int Renderer::drawCharacter(int x, int y, int UTF8Index, bool bold,bool italics, int size,bool color, bool uniFont)
{
    if(uniFont || !fontHandler.checkCharExist(UTF8Index)) //draw the character as unifont
    {
        const char* glyphData = fontHandler.getChar(UTF8Index);
        bool wideGlyph = fontHandler.getCharWidth(UTF8Index) -1;
        for(int i=0;i<16;i++)
        {
            for(int j=0;j<16;j++)
            {
                int coordx = x + (16-i)*size;
                int coordy = y + (16-j)*size;

                if(italics && j<4) coordx+= size;
                if(italics && j>=8) coordx-= size;
                if(italics && j>=12) coordx-= size;

                if(!wideGlyph)
                {
                int byteIndex = j + int(i<8)*16;
                int bitIndex = i%8;
                if((glyphData[byteIndex] >> bitIndex) & 1) this->drawRectangle(coordx,coordy,size,size,color);
                }
                else
                {
                int byteIndex = (j*2) + int(i<8);
                int bitIndex = i%8;
                if((glyphData[byteIndex] >> bitIndex) & 1) this->drawRectangle(coordx,coordy,size,size,color);
                }
            }
        }
        if(bold) this->drawCharacter(x+size,y,UTF8Index, false,italics,size,color);
        return (8 + 8*wideGlyph)*size;
    }
    else //draw the character from the loaded .yaff font
    {
        if(UTF8Index>9999 && fontHandler.uniFontEnabled==false) return 0;
        int width = fontHandler.getFontCharBitmapWidth(UTF8Index);
        int height = fontHandler.getFontCharHeight(UTF8Index);
        int italicsOffset = 0;
        for(int i=0;i<width;i++)
        {
            for(int j=0;j<height;j++)
            {
                if(italics) italicsOffset = (j-height/2)/2;
                int pixelIndex = i+(height-j-1)*width;
                if(fontHandler.getFontChar(UTF8Index,pixelIndex)) drawRectangle(x+(i+fontHandler.getLeftBearing(UTF8Index)+italicsOffset)*size,y+(j+fontHandler.getShiftUp(UTF8Index))*size,size,size,color);
                
            }
            
        }
        if(bold) this->drawCharacter(x+size,y,UTF8Index, false,italics,size,color,false);
        return (fontHandler.getFontCharWidth(UTF8Index) + bold)*size;
    }
}

void Renderer::drawText(int x, int y, int* text, int text_length, int size, bool* boldMask,bool* italicsMask)
{
    bool bold=false;
    bool italics=false;
    int currentCharacterX = x;
    for(int i=0;i<text_length;i++)
    {   
        if(boldMask) bold=boldMask[i];
        if(italicsMask) italics=italicsMask[i];
        currentCharacterX += this->drawCharacter(currentCharacterX,y,text[i],bold,italics,size);
    }
}

void Renderer::drawText(int x, int y, int* text, int text_length, int size, bool bold,bool italics)
{
    int currentCharacterX = x;
    for(int i=0;i<text_length;i++)
    {   
        currentCharacterX += this->drawCharacter(currentCharacterX,y,text[i],bold,italics,size);
    }
}

void Renderer::drawString(int x, int y,std::string textString, int size, bool bold,bool italics,bool color)
{
    int stringSize = (int) textString.length();
    int* intArray = new int[stringSize];
    int utf8StringSize = utf8ToCodePoints(textString.c_str(), intArray, stringSize);

    int currentCharacterX = x;
    for(int i=0;i<utf8StringSize;i++)
    {   
        currentCharacterX += this->drawCharacter(currentCharacterX,y,intArray[i],bold,italics,size,color);
    }
    delete[] intArray;
}

void Renderer::drawString(int x, int y, std::vector<int> intArray, int size, std::vector<int> boldMask,std::vector<int> italicsMask,bool color,bool uniFont)
{
    int currentCharacterX = x;
    for(int i=0;i<intArray.size();i++)
    {   
        currentCharacterX += this->drawCharacter(currentCharacterX,y,intArray[i],boldMask[i],italicsMask[i],size,color,uniFont);
    }
}


void Renderer::drawRectangle(int startx, int starty, int lengthx, int lengthy, bool ink)
{
    for(long i=0;i<lengthx;i++)
    {
        for(long j=0;j<lengthy;j++)
        {
            int coordx = startx + i;
            int coordy = starty + j;
            this->drawPixel(coordx,coordy,ink);
        }
    }
}

void Renderer::drawPaddedBox(int startx, int starty, int lengthx, int lengthy, int padding, bool value)
{
    if (!value) {
        // Just fill the whole box
        drawRectangle(startx, starty, lengthx, lengthy, true);
    } else {
        // Draw the border as four strips
        // Top
        drawRectangle(startx, starty, lengthx, padding, true);
        // Bottom
        drawRectangle(startx, starty + lengthy - padding, lengthx, padding, true);
        // Left
        drawRectangle(startx, starty + padding, padding, lengthy - 2 * padding, true);
        // Right
        drawRectangle(startx + lengthx - padding, starty + padding, padding, lengthy - 2 * padding, true);
    }
}

void Renderer::drawTextBox(int y, std::string line1, bool line1Bold,bool inverted)
{
    int originX = GLYPH_WIDTH;
    int rectHeight = 2 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    drawPaddedBox(originX,y,rectWidth,rectHeight,padding,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 0.5*GLYPH_HEIGHT + padding,line1,1,line1Bold,false,!inverted);
}

void Renderer::drawTextBox(int y, std::string line1, bool line1Bold,std::string line2, bool line2Bold,bool inverted)
{
    int originX = GLYPH_WIDTH;
    int rectHeight = 3 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    drawPaddedBox(originX,y,rectWidth,rectHeight,padding,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 1.5*GLYPH_HEIGHT + padding,line1,1,line1Bold,false,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 0.5*GLYPH_HEIGHT + padding,line2,1,line2Bold,false,!inverted);
}

void Renderer::drawTextBox(int y, std::string line1, bool line1Bold,std::string line2, bool line2Bold,std::string line3, bool line3Bold,bool inverted)
{
    int originX = GLYPH_WIDTH;
    int rectHeight = 4 * GLYPH_HEIGHT;
    int rectWidth = EPD_HEIGHT - 2 * originX;
    int padding = 4;
    drawPaddedBox(originX,y,rectWidth,rectHeight,padding,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 2.5*GLYPH_HEIGHT + padding,line1,1,line1Bold,false,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 1.5*GLYPH_HEIGHT + padding,line2,1,line2Bold,false,!inverted);
    drawString(originX + GLYPH_WIDTH/2, y + 0.5*GLYPH_HEIGHT + padding,line3,1,line3Bold,false,!inverted);
}

void Renderer::drawPixel(int x, int y, bool ink)
{
    if (x < 0 || x >= EPD_HEIGHT) return;
    if (y < 0 || y >= EPD_WIDTH) return;

    int flippedX = (EPD_HEIGHT - 1) - x;
    int flippedY = (EPD_WIDTH  - 1) - y;

    int byteLoc = (flippedY / 8) + flippedX * (EPD_WIDTH / 8);
    int bitLoc = 7 - (flippedY % 8);

    if (ink)
        framebuffer[byteLoc] |= (1 << bitLoc);
    else
        framebuffer[byteLoc] &= ~(1 << bitLoc);
}

void Renderer::drawBookMark(unsigned char* framebuffer,bool ink)
{
    this->framebuffer = framebuffer;
    int size = 50;
    for(int x=0;x<size;x++)
    {
        for(int y=0;y<size;y++)
        {
            if(y<x) drawPixel(x+EPD_HEIGHT-size,EPD_WIDTH-y,ink);
        }
    }

}

void Renderer::drawPageOverlay(unsigned char* framebuffer,int currentPage,int totalPages)
{
    this->framebuffer = framebuffer;
    std::string pageString;
    if(totalPages < 2) return;
    if(!Device::getInstance().deviceSettings.showPagePercentage) pageString = std::to_string(currentPage) + "/" + std::to_string(totalPages);
    else pageString = std::to_string(((currentPage-1)*100) / (totalPages-1)) + "%";
    int pageStringLength = pageString.length();
    int coordx = EPD_HEIGHT/2 - GLYPH_WIDTH/4 - GLYPH_WIDTH*pageStringLength/4;
    int coordy = 0;
    int *textArray = (int*) calloc(pageStringLength,sizeof(int));
    for(int i=0;i<pageStringLength;i++)
    {
        textArray[i] = static_cast<int>(pageString[i]);
    }
    this->drawText(coordx,coordy,textArray,pageStringLength,1,nullptr,nullptr);
    free(textArray);
}

void Renderer::drawVoltage(unsigned char* framebuffer,float voltage)
{
    this->framebuffer = framebuffer;
    std::string voltageString = std::to_string(voltage).substr(0, 4) + "V";
    int voltageStringLength = voltageString.length();
    int coordx = EPD_HEIGHT - GLYPH_WIDTH/2 -  (GLYPH_WIDTH*(voltageStringLength/2+4));
    int coordy = 0;
    this->drawString(coordx,coordy,voltageString,1,false,false);
}

void Renderer::drawBattery(unsigned char* framebuffer,int level)
{
    this->framebuffer = framebuffer;
    drawRectangle(EPD_HEIGHT-6*GLYPH_WIDTH,0,EPD_HEIGHT,GLYPH_HEIGHT,false);
    drawRectangle(0,0,6*GLYPH_WIDTH,GLYPH_HEIGHT,false);
    std::string voltageString = std::to_string(level)+ "%";
    int voltageStringLength = voltageString.length();
    int coordx = EPD_HEIGHT - GLYPH_WIDTH/2 -  (GLYPH_WIDTH*(voltageStringLength/2+1));
    int coordy = 0;
    if (Device::getInstance().deviceSettings.displayBattery) {
        if(Device::getInstance().usb_state==Device::usbState::Charging || Device::getInstance().usb_state==Device::usbState::Query) this->drawCharacter(coordx-GLYPH_WIDTH,coordy,128268);
        else if(level>20) this->drawCharacter(coordx-GLYPH_WIDTH,coordy,128267);
        else this->drawCharacter(coordx-GLYPH_WIDTH,coordy,129707);
        this->drawString(coordx,coordy,voltageString,1,false,false);
    }
    if(Device::getInstance().deviceSettings.sunlightMode) {
        this->drawCharacter(GLYPH_WIDTH,coordy,9788);
    }
}

void Renderer::clearScreenBuffer(void)
{
    for(int i=0;i<EPD_WIDTH*EPD_HEIGHT/8;i++)
    {
        this->framebuffer[i] = 0x00;
    }
}

void Renderer::clearScreenBuffer(unsigned char* framebuffer)
{
    for(int i=0;i<EPD_WIDTH*EPD_HEIGHT/8;i++)
    {
        framebuffer[i] = 0x00;
    }
}

void Renderer::drawClearScreen(bool screenID)
{
    epd.Clear(screenID);
}

void Renderer::drawScreen(bool screenID)
{
    ESP_LOGE(TAG, "drawing screenbuffer");
    epd.DisplayPicture(screenID, this->framebuffer);
}

void Renderer::drawImage(Image image,int startX, int startY)
{
    if (image.imageData.empty() || image.imageWidth == 0 || image.imageHeight == 0) {
    ESP_LOGE(TAG, "No image data available for drawing");
    return;
    }
    int w = image.imageWidth;
    int h = image.imageHeight;
    std::vector<uint8_t> pixels = image.imageData;
    for(int x=0;x<w;x++)
    {
        for(int y=0;y<h;y++)
        {
            bool pixelVal = !(pixels[x+y*w]>250);
            this->drawPixel(x+startX,EPD_WIDTH-y-startY,pixelVal);
        }

    }
}


void Renderer::deepSleep(void)
{
    epd.DeepSleep(true);
    epd.DeepSleep(false);
}

void Renderer::drawScreenPartial(bool screenID,int partialMinX,int partialMinY,int partialMaxX,int partialMaxY)
{
    ESP_LOGE(TAG, "drawing partial screenbuffer");
    epd.DisplayPicturePartial(screenID, this->framebuffer, partialMinX, partialMinY, partialMaxX, partialMaxY);
}

void Renderer::drawProgressBar(int y,int length,int percent)
{
    int solidSquares = (length * percent)/100;
    int greySquares = length - solidSquares;
    int startX = (EPD_HEIGHT/2)-length*GLYPH_WIDTH/4;
    int currentX = startX;
    for(int i=0;i<length;i++)
    {   
        if(i<solidSquares) currentX += this->drawCharacter(currentX,y,9608,false,false,1);
        else currentX += this->drawCharacter(currentX,y,9617,false,false,1);
    }

}

void Renderer::drawRightArrow(int xTip, int yTip, int armLength, int height, int thickness, bool ink)
{
    if(armLength>1) drawRectangle(xTip-armLength,yTip-thickness+1,armLength-2,2*thickness,ink);
    int diagonalThickness = 3 * thickness;
    for(int i=0;i<height;i++)
    {
        int yTop = yTip + i;
        int yBottom = yTip - i;
        int x = xTip - i;
        drawRectangle(x-diagonalThickness,yTop,diagonalThickness,1,ink);
        drawRectangle(x-diagonalThickness,yBottom,diagonalThickness,1,ink);
    }
}

void Renderer::drawUpArrow(int xTip, int yTip, int armLength, int height, int thickness, bool ink)
{
    if(armLength>1) drawRectangle(xTip-thickness+1,yTip-armLength,2*thickness,armLength-2,ink);
    int diagonalThickness = 3 * thickness;
    for(int i=0;i<height;i++)
    {
        int xTop = xTip + i;
        int xBottom = xTip - i;
        int y = yTip - i;
        drawRectangle(xBottom,y-diagonalThickness,1,diagonalThickness,ink);
        drawRectangle(xTop,y-diagonalThickness,1,diagonalThickness,ink);
    }
}

void Renderer::drawDownArrow(int xTip, int yTip, int armLength, int height, int thickness, bool ink)
{
    if(armLength>1) drawRectangle(xTip-thickness+1,yTip+2,2*thickness,armLength,ink);
    int diagonalThickness = 3 * thickness;
    for(int i=0;i<height;i++)
    {
        int xTop = xTip + i;
        int xBottom = xTip - i;
        int y = yTip + i;
        drawRectangle(xBottom,y,1,diagonalThickness,ink);
        drawRectangle(xTop,y,1,diagonalThickness,ink);
    }
}