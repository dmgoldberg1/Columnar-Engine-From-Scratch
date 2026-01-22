#include "utilities.h"

#include <regex>

bool isInteger(const std::string& str) {
    std::regex integer_regex("^[+-]?\\d+$");
    return std::regex_match(str, integer_regex);
}