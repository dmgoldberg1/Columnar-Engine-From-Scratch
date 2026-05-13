#pragma once

#include <string>
#include <cstdint>


enum class Types : int64_t {
    TypeInt16 = 1,
    TypeInt32 = 2,
    TypeInt64 = 3,
    TypeString = 4,
    TypeDouble = 5,
    TypeDateTime = 6,
    TypeTimestamp = 7
};

bool isInteger(const std::string& str);
bool isDateTime(const std::string& str);
uint32_t ParseDateTime(const std::string& str);
std::string FormatDateTime(uint32_t value);
void WriteNum(int64_t num, std::ostream& output);
uint64_t HashInt64(int64_t x);
uint64_t HashDouble(double x);
uint64_t HashString(const std::string& x);
uint64_t HashCombine(uint64_t seed, uint64_t value);
