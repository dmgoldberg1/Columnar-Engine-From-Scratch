#include "utilities.h"

#include <regex>
#include <cstring>

bool isInteger(const std::string& str) {
    std::regex integer_regex("^[+-]?\\d+$");
    return std::regex_match(str, integer_regex);
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