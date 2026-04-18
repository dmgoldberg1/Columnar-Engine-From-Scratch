#pragma once

#include <string>

enum class Types : int64_t {
    TypeInt64 = 1,
    TypeString = 2,
    TypeDouble = 3
};

bool isInteger(const std::string& str);
void WriteNum(int64_t num, std::ostream& output);