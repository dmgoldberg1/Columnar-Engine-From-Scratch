#pragma once

#include <string>
#include <cstdint>


enum class Types : int64_t {
    TypeInt64 = 1,
    TypeString = 2,
    TypeDouble = 3
};

bool isInteger(const std::string& str);
void WriteNum(int64_t num, std::ostream& output);
uint64_t HashInt64(int64_t x);
uint64_t HashDouble(double x);
uint64_t HashString(const std::string& x);
uint64_t HashCombine(uint64_t seed, uint64_t value);