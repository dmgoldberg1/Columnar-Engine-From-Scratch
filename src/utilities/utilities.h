#pragma once

#include <string>
#include <cstdint>
#include <vector>


enum class Types : int64_t {
    TypeInt16 = 1,
    TypeInt32 = 2,
    TypeInt64 = 3,
    TypeString = 4,
    TypeDouble = 5,
    TypeDate = 6,
    TypeTimestamp = 7
};

bool isDate(const std::string& str);
uint32_t ParseDate(const std::string& str);
std::string FormatDate(uint32_t value);
bool isTimestamp(const std::string& str);
uint32_t ParseTimestamp(const std::string& str);
std::string FormatTimestamp(uint32_t value);
const std::vector<std::string>& GetHitsColumnNames();
uint64_t HashInt64(int64_t x);
uint64_t HashDouble(double x);
uint64_t HashString(const std::string& x);
uint64_t HashCombine(uint64_t seed, uint64_t value);
