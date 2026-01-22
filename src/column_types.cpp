#include "column_types.h"

#include <stdexcept>
#include <cstring>

void Int64::Write(std::ostream& output) {
    for (int64_t val : value_) {
        output.write(reinterpret_cast<const char*>(&val), sizeof(val));
    }
}

void Int64::AddCell(const std::unique_ptr<Column>& cell) {
    Int64* int64_cell = dynamic_cast<Int64*>(cell.get());
    if (!int64_cell) {
        throw std::runtime_error("Cannot add a cell to Int64 column.");
    }
    value_.push_back(int64_cell->GetFirstCellValue());
}

void Int64::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(int64_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

void String::Write(std::ostream& output) {
    for (const auto& val : value_) {
        int64_t length = val.size();
        output.write(reinterpret_cast<const char*>(&length), sizeof(length));
        output.write(val.data(), length * sizeof(char));
    }
}

void String::AddCell(const std::unique_ptr<Column>& cell) {
    String* string_cell = dynamic_cast<String*>(cell.get());
    if (!string_cell) {
        throw std::runtime_error("Cannot add a cell to String column.");
    }
    size_ += sizeof(int64_t) + string_cell->GetFirstCellValue().size() * sizeof(char);
    value_.push_back(std::move(string_cell->GetFirstCellValue()));
    
}

void String::SetData(const std::vector<uint8_t>& data) {
    auto it = data.begin();
    while (it != data.end()) {
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

size_t Int64::GetColumnByteSize() const {
    return value_.size() * sizeof(int64_t);
    
}
