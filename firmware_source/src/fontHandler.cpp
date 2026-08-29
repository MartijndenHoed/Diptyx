#include "fontHandler.h"
#include "esp_log.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include "cJSON.h"
#include <dirent.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "fontHandler";

extern "C" {
extern const uint8_t binary_unifont_hex_start[] asm("_binary_unifont_hex_start");
extern const uint8_t binary_unifont_hex_end[] asm("_binary_unifont_hex_end");

extern const uint8_t binary_charWidths_hex_start[] asm("_binary_charWidths_hex_start");
extern const uint8_t binary_charWidths_hex_end[] asm("_binary_charWidths_hex_end");
}

const uint8_t* FontHandler::start() {
    return binary_unifont_hex_start;
}

const uint8_t* FontHandler::startCharWidths() {
    return binary_charWidths_hex_start;
}

const uint8_t* FontHandler::end() {
    return binary_unifont_hex_end;
}


size_t FontHandler::size() {
    return end() - start();
}

int FontHandler::getCharWidth(int index)
{
    const uint8_t* data = this->startCharWidths();
    int byteIndex = index/8;
    int bitIndex = index%8;
    return ((data[byteIndex] >> bitIndex) & (uint8_t)1)+1;
}

const char* FontHandler::getChar(int index)
{
    const uint8_t* data = this->start();
    return reinterpret_cast<const char*>(&data[32 * index]);
}

int FontHandler::getFontCharWidth(int index)
{
    if(uniFontEnabled) return getCharWidth(index)*8;
    if(!checkCharExist(index)) return getCharWidth(index)*8;
    if(index>9999) return 0;
    return static_cast<int> (width[index]);
}

int FontHandler::getFontCharBitmapWidth(int index)
{
    if(uniFontEnabled) return getCharWidth(index)*8;
    if(index>9999) return 0;
    return static_cast<int> (width[index] - leftBearing[index] - rightBearing[index]);
}

int FontHandler::getFontCharHeight(int index)
{
    if(uniFontEnabled) return 16;
    return currentFont.boundingBoxY;
}

int FontHandler::getLineHeight()
{
    if(uniFontEnabled) return 16;
    return currentFont.lineHeight;
}

int FontHandler::getLeftBearing(int index)
{
    if(uniFontEnabled) return 0;
    return leftBearing[index];
}

bool FontHandler::getFontChar(int index,int pixel)
{
    if(uniFontEnabled) 
    {
        const char* glyphAddress = getChar(index);
        int byteIndex = pixel/8;
        int bitIndex = 7-pixel%8;
        return (glyphAddress[byteIndex] >> bitIndex) & 1;
    }
    if(index>9999) return 0;
    int address = bitmapMap[index] + pixel;
    int byteIndex = address/8;
    int bitIndex = 7-address%8;
    return (bitmap[byteIndex] >> bitIndex) & 1;
}

bool FontHandler::checkCharExist(int index)
{
    if(uniFontEnabled) return true;
    if(index<=9999 && bitmapMap[index]) return true;
    return false;
}


void FontHandler::testPrint() {
    const uint8_t* data = this->start();
    size_t len = this->size();

    for (size_t i = 0; i < 32; i++) {
        printf("%02X ", data[32*65 + i]);
    }
}


void FontHandler::indexFonts() {
    fonts.clear();
    families.clear();

    font uniFont = font("UniFont","UniFont","UniFont",16,16,16,11,0);
    fonts.push_back(uniFont);


    const char *fontsDir = "/sdcard/fonts";
    DIR *dir = opendir(fontsDir);
    if (!dir) {
    if (errno == ENOENT) {
        ESP_LOGW(TAG, "Fonts directory does not exist, creating: %s", fontsDir);

        if (mkdir(fontsDir, 0755) != 0) {
            ESP_LOGE(TAG, "Failed to create directory %s (errno=%d)", fontsDir, errno);
            return;
        }

        dir = opendir(fontsDir);
        if (!dir) {
            ESP_LOGE(TAG, "Created directory but could not open %s (errno=%d)",
                     fontsDir, errno);
            return;
        }

        ESP_LOGI(TAG, "Created fonts directory: %s", fontsDir);
        } else {
            ESP_LOGE(TAG, "Could not open fonts directory %s (errno=%d)",
                    fontsDir, errno);
            return;
        }
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fname(entry->d_name);

        // skip . and ..
        if (fname == "." || fname == "..") continue;

        // only process .yaff files
        if (fname.size() > 5 && fname.substr(fname.size() - 5) == ".yaff") {
            std::string fontName = fname.substr(0, fname.size() - 5); // strip extension
            indexFont(fontName);
        }
    }

    closedir(dir);

    // Build family list
    for (const auto &f : fonts) {
        auto it = std::find_if(families.begin(), families.end(),
                               [&](const fontFamily &fam) { return fam.name == f.family; });
        if (it != families.end()) {
            it->fonts.push_back(f);
        } else {
            fontFamily fam;
            fam.name = f.family;
            fam.fonts.push_back(f);
            families.push_back(fam);
        }
    }

    // Sort fonts inside each family by point size
    for (auto &fam : families) {
        std::sort(fam.fonts.begin(), fam.fonts.end(),
                  [](const font &a, const font &b) {
                      return a.pointSize < b.pointSize;
                  });
    }

    printf("Indexed %zu fonts into %zu families from %s\n",
           fonts.size(), families.size(), fontsDir);
}


void FontHandler::indexFont(const std::string &fontName) {
    std::string yaffPath = "/sdcard/fonts/" + fontName + ".yaff";
    std::ifstream file(yaffPath);
    if (!file.is_open()) {
        printf("Could not open YAFF file: %s\n", yaffPath.c_str());
        return;
    }

    font f;
    f.fileName = fontName;

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;

        // Only top-level unindented properties
        if (line[0] != ' ' && line[0] != '\t') {
            auto pos = line.find(':');
            if (pos == std::string::npos) continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            // trim
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            // map relevant fields
            if (key == "name") f.name = value;
            else if (key == "family") f.family = value;
            else if (key == "point-size") f.pointSize = std::stoi(value);
            else if (key == "line-height") f.lineHeight = std::stoi(value);
            else if (key == "shift-up") f.shiftUp = std::stoi(value);
            else if (key == "bounding-box") {
                int x = 0, y = 0;
                int n = sscanf(value.c_str(), "%d %d", &x, &y);
                if (n != 2) {
                    // try "20x20" style
                    n = sscanf(value.c_str(), "%dx%d", &x, &y);
                }
                f.boundingBoxX = x;
                f.boundingBoxY = y;
            }
            else if (key == "cell-size") {
                int x = 0, y = 0;
                int n = sscanf(value.c_str(), "%d %d", &x, &y);
                if (n != 2) {
                    // try "20x20" style
                    n = sscanf(value.c_str(), "%dx%d", &x, &y);
                }
                f.boundingBoxX = x;
                f.boundingBoxY = y;
            }
        } else {
            // indented lines are glyphs, stop reading metadata
            break;
        }
    }

    file.close();

    fonts.push_back(f);
    printf("Indexed font from YAFF: %s (fileName: %s, pointSize: %d)\n",
           f.name.c_str(), f.fileName.c_str(), f.pointSize);
}


template <typename T>
bool FontHandler::readFileToBuffer(const std::string &path, std::vector<T> &buffer) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        printf("Could not open file: %s\n", path.c_str());
        return false;
    }

    std::streamsize size = file.tellg();
    if (size % sizeof(T) != 0) {
        printf("File size mismatch for %s (expected multiple of %zu, got %lld)\n",
               path.c_str(), sizeof(T), (long long)size);
        return false;
    }

    file.seekg(0, std::ios::beg);
    buffer.resize(size / sizeof(T));

    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        printf("Failed to read file: %s\n", path.c_str());
        return false;
    }
    return true;
}

int FontHandler::parseCodepoint(const std::string &tok) {
    std::string t = tok;
    for (auto &c : t) c = std::tolower(c);

    if (t.rfind("u+", 0) == 0) return std::stoi(t.substr(2), nullptr, 16);
    //if (t.rfind("0x", 0) == 0) return std::stoi(t.substr(2), nullptr, 16);
    //if (std::all_of(t.begin(), t.end(), ::isdigit)) return std::stoi(t);
    return -1;
}

void FontHandler::processGlyph(int codepoint,
                               const std::vector<std::string> &bitmapLines,
                               size_t &pixelIndex,
                               int leftBearingVal,
                               int rightBearingVal) {
    if (codepoint < 0 || codepoint >= 10000) return;
    // Convert to binary grid
    int height = bitmapLines.size();
    int widthPx = 0;
    std::vector<std::vector<int>> grid;
    for (auto &ln : bitmapLines) {
        std::vector<int> row;
        for (char ch : ln) {
            if (ch == '@') row.push_back(1);
            else if (ch == '.') row.push_back(0);
        }
        if (!row.empty()) {
            widthPx = std::max(widthPx, (int)row.size());
            grid.push_back(row);
        }
    }

    // Set provided values
    int left = leftBearingVal >= 0 ? leftBearingVal : 0;
    int right = rightBearingVal >= 0 ? rightBearingVal : 0;

    int charWidth = widthPx;  // real pixel width
    width[codepoint] = charWidth+left+right;
    leftBearing[codepoint] = left;
    rightBearing[codepoint] = right;

    // Record start index
    bitmapMap[codepoint] = pixelIndex;

    // printf("codepoint %d \n",codepoint);
    // printf("charWidth %d \n",widthPx);
    // printf("leftBearing %d \n",left);

    // Pack bits into bitmap vector
    for (auto &row : grid) {
        for (int bit : row) {
            size_t byteIndex = pixelIndex / 8;
            int bitIndex = 7 - (pixelIndex % 8);

            if (byteIndex >= bitmap.size()) {
                bitmap.push_back(0);
            }
            bitmap[byteIndex] |= (bit & 1) << bitIndex;

            pixelIndex++;
        }
    }
}

void FontHandler::loadFont(const std::string &yaffPath) {
    if(yaffPath=="UniFont")
    {
        uniFontEnabled = true;
        currentFont = fonts[0];
        return;
    }
    uniFontEnabled = false;
    auto it = std::find_if(fonts.begin(), fonts.end(),
                           [&](const font &f) { return f.fileName == yaffPath; });

    if (it == fonts.end()) {
        printf("Font %s not found in indexed list\n", yaffPath.c_str());
        return;
    }
    
    // set currentFont metadata
    currentFont = *it;

    // Reset buffers
    width.assign(10000, 0);
    leftBearing.assign(10000, 0);
    rightBearing.assign(10000, 0);
    bitmapMap.assign(10000, 0);
    bitmap.clear();

    // Read file
    std::ifstream file("/sdcard/fonts/" + yaffPath + ".yaff");
    if (!file.is_open()) {
        printf("Could not open YAFF file: %s\n", yaffPath.c_str());
        return;
    }

    std::string line;
    std::string currentLabel;
    std::vector<std::string> bitmapLines;
    int codepoint = -1;
    size_t pixelIndex = 1;
    int leftVal = -1, rightVal = -1;

    while (std::getline(file, line)) {
        // trim leading/trailing spaces
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) line.clear();
        else line = line.substr(start);

        if (line.empty() || line[0] == '#') continue;

        // Glyph label like "u+0041:"
        if (line.back() == ':') {
            // If previous glyph exists, flush it
            if (line.rfind("u+", 0) == 0)
            {
                if (codepoint >= 0 && (!bitmapLines.empty() || leftVal>-1 || rightVal>-1)) {
                    processGlyph(codepoint, bitmapLines, pixelIndex, leftVal, rightVal);
                    bitmapLines.clear();
                    leftVal = rightVal = -1;
                    codepoint=-1;
                }
            }

            currentLabel = line.substr(0, line.size() - 1);
            if(codepoint==-1) codepoint = parseCodepoint(currentLabel);
            continue;
        }

        // Bearing values
        if (line.rfind("left-bearing:", 0) == 0) {
            leftVal = std::stoi(line.substr(13));
            continue;
        }
        if (line.rfind("right-bearing:", 0) == 0) {
            rightVal = std::stoi(line.substr(14));
            continue;
        }

        // Bitmap line
        if (!line.empty() && (line.find_first_of(".@") ==0) && codepoint!=-1) {
            bitmapLines.push_back(line);
        }
    }

    // Flush last glyph
    if (codepoint >= 0 && !bitmapLines.empty()) {
        processGlyph(codepoint, bitmapLines, pixelIndex, leftVal, rightVal);
    }

    file.close();

    printf("Loaded YAFF font: %s\n", yaffPath.c_str());
    printf("  bitmap: %zu bytes\n", bitmap.size());
}
