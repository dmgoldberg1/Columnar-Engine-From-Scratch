#include "column_types.h"

#include <stdexcept>
#include <string_view>
#include <cstring>

void Int64::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(int64_t) * value_.size());
}

void Int64::AddCell(const std::string& cell) {
    try {
        int64_t num = std::stoll(cell);
        value_.push_back(num);
    } catch (...) {
        throw std::runtime_error("Cannot add cell to Int64 column.");
    }
}

void Int64::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(int64_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

std::vector<std::string> Int64::GetColumnAsString() const {
    std::vector<std::string> result;
    for (int64_t i = 0; i < value_.size(); ++i) {
        std::string val = GetCellAsString(i);
        result.push_back(std::move(val));
    }
    return result;
}

size_t Int64::GetColumnByteSize() const {
    return value_.size() * sizeof(int64_t);
}

void String::Write(std::ostream& output) {
    std::vector<uint8_t> serialization;
    for (const auto& val : value_) {
        int64_t str_size = val.size();
        uint8_t* size_bytes = reinterpret_cast<uint8_t*>(&str_size);
        serialization.insert(serialization.end(), size_bytes, size_bytes + sizeof(int64_t));
        serialization.insert(serialization.end(), val.begin(), val.end());
    }
    output.write(reinterpret_cast<const char*>(serialization.data()), serialization.size());
}

void String::AddCell(const std::string& cell) {
    size_ += sizeof(int64_t) + cell.size();
    value_.push_back(std::move(cell));
    
}

void String::SetData(const std::vector<uint8_t>& data) {
    auto it = data.begin();
    int loop_count = 1;
    while (it != data.end()) {
        ++loop_count;
        int64_t str_size;
        std::memcpy(&str_size, &(*it), sizeof(int64_t));
        it += sizeof(int64_t);
        std::string str;
        str.reserve(str_size);
        for (int64_t i = 0; i < str_size; ++i) {
            str.push_back(static_cast<char>(*it));
            ++it;
        }
        value_.push_back(std::move(str));
    }
}

std::vector<std::string> String::GetColumnAsString() const {
    return value_;
}





