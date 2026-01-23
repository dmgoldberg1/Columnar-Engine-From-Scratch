#include "utilities.h"

#include <regex>

bool isInteger(const std::string& str) {
    std::regex integer_regex("^[+-]?\\d+$");
    return std::regex_match(str, integer_regex);
}

void WriteNum(int64_t num, std::ostream& output) {
    output.write(reinterpret_cast<const char*>(&num), sizeof(num));
}