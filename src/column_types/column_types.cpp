#include "column_types.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <cstring>
#include <iostream>

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

bool Int64::Compare(int row, Op op, CellTypes val) const {
    int64_t lhs = value_.at(row);
    int64_t rhs = std::get<int64_t>(val);
    switch (op) {
        case Op::EQ:
            return lhs == rhs;
        case Op::NE:
            return lhs != rhs;
        case Op::LT: 
            return lhs < rhs;
        case Op::LE:
            return lhs <= rhs;
        case Op::GT:
            return lhs > rhs;
        case Op::GE: 
            return lhs >= rhs;
    }
    return false;
}

void Int64::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<int64_t> new_values;
    new_values.reserve(mask.size());
    int64_t i = 0;
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
        ++i;
    }
    value_ = std::move(new_values);
}

int64_t Int64::GetSum() const {
    int64_t ans = 0;
    for (auto el : value_) {
        ans += el;
    }
    return ans;
}

CellTypes Int64::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return *it;
}

CellTypes Int64::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
}

void Int64::FillHashSet(std::unordered_set<int64_t>& set) const {
    for (const auto& el : value_) {
        set.insert(el);
    }
}

void Int64::AddCell(const CellTypes& cell) {
    int64_t val = std::get<int64_t>(cell);
    value_.push_back(val);
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

bool String::Compare(int row, Op op, CellTypes value) const {
    std::string lhs = value_.at(row);
    std::string rhs = std::get<std::string>(value);
    switch (op) {
        case Op::EQ:
            return lhs == rhs;
        case Op::NE:
            return lhs != rhs;
        case Op::LT: 
            return lhs < rhs;
        case Op::LE:
            return lhs <= rhs;
        case Op::GT:
            return lhs > rhs;
        case Op::GE: 
            return lhs >= rhs;
    }
    return false;
}

void String::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<std::string> new_values;
    new_values.reserve(mask.size());
    int64_t i = 0;
    for (int64_t id : mask) {
        new_values.push_back(std::move(value_[id]));
        ++i;
    }
    value_ = std::move(new_values);
}

CellTypes String::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return *it;
}

CellTypes String::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
}

void String::FillHashSet(std::unordered_set<std::string>& set) const {
    for (const auto& el : value_) {
        set.insert(el);
    }
}

void String::AddCell(const CellTypes& cell) {
    std::string val = std::get<std::string>(cell);
    value_.emplace_back(val);
}

Double::Double(double value) {
    value_.push_back(value);
}

void Double::Write(std::ostream& output) {
    // TODO: реализация записи
}

void Double::AddCell(const std::string& cell) {
    // TODO: парсинг строки в double
}

int64_t Double::GetLastCellSize() const {
    return 0;
}

size_t Double::GetColumnByteSize() const {
    return 0;
}

std::string Double::GetCellAsString(int64_t i) const {
    return "";
}

std::vector<std::string> Double::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (double val : value_) {
        result.push_back(std::to_string(val));
    }
    return result;
}

int64_t Double::GetRowCount() const {
    return 0;
}

CellTypes Double::GetMin() const {
    return 0.0;
}

CellTypes Double::GetMax() const {
    return 0.0;
}

bool Double::Compare(int row, Op op, CellTypes val) const {
    return false;
}

void Double::FilterRows(const std::vector<int64_t>& mask) {
    // TODO: фильтрация вектора
}

void Double::Clear() {
    value_.clear();
}

void Double::SetData(const std::vector<uint8_t>& data) {
    // TODO: загрузка сырых байт
}

void Double::AddCell(const CellTypes& cell) {
    double val = std::get<double>(cell);
    value_.push_back(val);
}





