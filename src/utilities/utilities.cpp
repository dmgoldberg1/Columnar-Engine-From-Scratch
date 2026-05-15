#include "utilities.h"

#include <ctime>
#include <cctype>
#include <cstring>
#include <ostream>
#include <stdexcept>

bool isInteger(const std::string& str) {
    if (str.empty()) {
        return false;
    }
    size_t start = 0;
    if (str[0] == '+' || str[0] == '-') {
        if (str.size() == 1) {
            return false;
        }
        start = 1;
    }
    for (size_t i = start; i < str.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}

bool isDate(const std::string& str) {
    if (str.size() != 10 || str[4] != '-' || str[7] != '-') {
        return false;
    }
    for (size_t i = 0; i < str.size(); ++i) {
        if (i == 4 || i == 7) {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}

uint32_t ParseDate(const std::string& str) {
    if (!isDate(str)) {
        throw std::runtime_error("Invalid date string: " + str);
    }
    std::tm tm = {};
    tm.tm_year = std::stoi(str.substr(0, 4)) - 1900;
    tm.tm_mon = std::stoi(str.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(str.substr(8, 2));
    tm.tm_isdst = -1;
    std::time_t timestamp = std::mktime(&tm);
    if (timestamp < 0) {
        throw std::runtime_error("Cannot parse date string: " + str);
    }
    return static_cast<uint32_t>(timestamp / 86400);
}

std::string FormatDate(uint32_t value) {
    std::time_t timestamp = static_cast<std::time_t>(value) * 86400;
    std::tm tm = *std::localtime(&timestamp);
    char buffer[11];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm) == 0) {
        throw std::runtime_error("Cannot format date value.");
    }
    return buffer;
}

bool isTimestamp(const std::string& str) {
    if (str.size() != 19 || str[4] != '-' || str[7] != '-' || str[10] != ' ' || str[13] != ':' || str[16] != ':') {
        return false;
    }
    for (size_t i = 0; i < str.size(); ++i) {
        if (i == 4 || i == 7 || i == 10 || i == 13 || i == 16) {
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(str[i]))) {
            return false;
        }
    }
    return true;
}

uint32_t ParseTimestamp(const std::string& str) {
    if (!isTimestamp(str)) {
        throw std::runtime_error("Invalid timestamp string: " + str);
    }
    std::tm tm = {};
    tm.tm_year = std::stoi(str.substr(0, 4)) - 1900;
    tm.tm_mon = std::stoi(str.substr(5, 2)) - 1;
    tm.tm_mday = std::stoi(str.substr(8, 2));
    tm.tm_hour = std::stoi(str.substr(11, 2));
    tm.tm_min = std::stoi(str.substr(14, 2));
    tm.tm_sec = std::stoi(str.substr(17, 2));
    tm.tm_isdst = -1;
    std::time_t timestamp = std::mktime(&tm);
    if (timestamp < 0) {
        throw std::runtime_error("Cannot parse timestamp string: " + str);
    }
    return static_cast<uint32_t>(timestamp);
}

std::string FormatTimestamp(uint32_t value) {
    std::time_t timestamp = static_cast<std::time_t>(value);
    std::tm tm = *std::localtime(&timestamp);
    char buffer[20];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm) == 0) {
        throw std::runtime_error("Cannot format timestamp value.");
    }
    return buffer;
}

void WriteNum(int64_t num, std::ostream& output) {
    output.write(reinterpret_cast<const char*>(&num), sizeof(num));
}

uint64_t HashInt64(int64_t x) {
    uint64_t z = static_cast<uint64_t>(x);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    return z;
}

uint64_t HashDouble(double x) {
    if (x == 0.0) {
        x = 0.0;
    }
    uint64_t z;
    std::memcpy(&z, &x, sizeof(x));
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    z = z ^ (z >> 31);
    return z;
}

uint64_t HashString(const std::string& s) {
    uint64_t h = 0x100;
    const uint64_t P = 131;

    for (unsigned char c : s) {
        h = h * P + c;
    }
    return HashInt64(h);
}

uint64_t HashCombine(uint64_t seed, uint64_t value) {
    seed ^= value + 0x9e3779b97f617dbULL + (seed << 6) + (seed >> 2);
    return seed;
}
