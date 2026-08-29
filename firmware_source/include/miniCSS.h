#pragma once

#include <string>
#include <unordered_map>

using StyleMap = std::unordered_map<std::string, std::string>;
using CSSCache = std::unordered_map<std::string, StyleMap>;

std::string trim(const std::string& str);
CSSCache parse_css_string(const std::string& css);
void merge_css(CSSCache& base, const CSSCache& other);