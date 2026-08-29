#include "miniCSS.h"
#include <sstream>
#include <algorithm>

std::string trim(const std::string& str) {
    const auto begin = str.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) return "";
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(begin, end - begin + 1);
}

CSSCache parse_css_string(const std::string& css) {
    CSSCache cache;

    size_t pos = 0;
    while ((pos = css.find('.', pos)) != std::string::npos) {
        size_t classStart = pos + 1;
        size_t classEnd = css.find_first_of(" {", classStart);
        if (classEnd == std::string::npos) break;

        std::string className = trim(css.substr(classStart, classEnd - classStart));
        size_t braceOpen = css.find('{', classEnd);
        size_t braceClose = css.find('}', braceOpen);
        if (braceOpen == std::string::npos || braceClose == std::string::npos) break;

        std::string block = css.substr(braceOpen + 1, braceClose - braceOpen - 1);
        std::istringstream rules(block);
        std::string line;
        StyleMap styles;

        while (std::getline(rules, line, ';')) {
            size_t colon = line.find(':');
            if (colon == std::string::npos) continue;

            std::string key = trim(line.substr(0, colon));
            std::string value = trim(line.substr(colon + 1));
            if (!key.empty() && !value.empty()) {
                styles[key] = value;
            }
        }

        if (!className.empty() && !styles.empty()) {
            cache[className] = styles;
        }

        pos = braceClose + 1;
    }

    return cache;
}

void merge_css(CSSCache& base, const CSSCache& other) {
    for (const auto& [className, styles] : other) {
        auto& target = base[className];
        for (const auto& [key, value] : styles) {
            target[key] = value;  // overwrite if already exists
        }
    }
}