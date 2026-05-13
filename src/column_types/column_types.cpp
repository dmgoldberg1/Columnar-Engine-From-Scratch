#include "column_types.h"

#include "../utilities/utilities.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <cstring>
#include <iostream>
#include <limits>

namespace {

uint64_t HashCell(const CellTypes& value) {
    return std::visit([](auto&& arg) -> uint64_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return HashInt64(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return HashString(arg);
        } else {
            return HashDouble(arg);
        }
    }, value);
}

std::string CellToString(const CellTypes& value) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else {
            return std::to_string(arg);
        }
    }, value);
}

}  // namespace

void Int16::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(int16_t) * value_.size());
}

void Int16::AddCell(const std::string& cell) {
    try {
        value_.push_back(static_cast<int16_t>(std::stoi(cell)));
    } catch (...) {
        value_.push_back(0);
    }
}

void Int16::AddCell(const CellTypes& cell) {
    value_.push_back(static_cast<int16_t>(std::get<int64_t>(cell)));
}

void Int16::AddColumn(const std::vector<std::string>& col) {
    for (const auto& cell : col) {
        AddCell(cell);
    }
}

size_t Int16::GetColumnByteSize() const {
    return value_.size() * sizeof(int16_t);
}

std::vector<std::string> Int16::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (int16_t value : value_) {
        result.push_back(std::to_string(value));
    }
    return result;
}

int64_t Int16::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes Int16::GetMin() const {
    return static_cast<int64_t>(*std::min_element(value_.begin(), value_.end()));
}

CellTypes Int16::GetMin(const std::vector<uint64_t>& mask) const {
    int16_t ans = std::numeric_limits<int16_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

CellTypes Int16::GetMax() const {
    return static_cast<int64_t>(*std::max_element(value_.begin(), value_.end()));
}

CellTypes Int16::GetMax(const std::vector<uint64_t>& mask) const {
    int16_t ans = std::numeric_limits<int16_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

bool Int16::Compare(int row, Op op, CellTypes value) const {
    int16_t lhs = value_.at(row);
    int16_t rhs = static_cast<int16_t>(std::get<int64_t>(value));
    switch (op) {
        case Op::EQ: return lhs == rhs;
        case Op::NE: return lhs != rhs;
        case Op::LT: return lhs < rhs;
        case Op::LE: return lhs <= rhs;
        case Op::GT: return lhs > rhs;
        case Op::GE: return lhs >= rhs;
    }
    return false;
}

void Int16::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes current = transform ? transform(CellTypes(static_cast<int64_t>(value_[i]))) : CellTypes(static_cast<int64_t>(value_[i]));
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

void Int16::FillHashSet(std::unordered_set<int64_t>& set) const {
    for (int16_t value : value_) {
        set.insert(static_cast<int64_t>(value));
    }
}

void Int16::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const {
    for (uint64_t id : mask) {
        set.insert(static_cast<int64_t>(value_[id]));
    }
}

void Int16::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<int16_t> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void Int16::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(int16_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

void Int32::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(int32_t) * value_.size());
}

void Int32::AddCell(const std::string& cell) {
    try {
        value_.push_back(static_cast<int32_t>(std::stol(cell)));
    } catch (...) {
        value_.push_back(0);
    }
}

void Int32::AddCell(const CellTypes& cell) {
    value_.push_back(static_cast<int32_t>(std::get<int64_t>(cell)));
}

void Int32::AddColumn(const std::vector<std::string>& col) {
    for (const auto& cell : col) {
        AddCell(cell);
    }
}

size_t Int32::GetColumnByteSize() const {
    return value_.size() * sizeof(int32_t);
}

std::vector<std::string> Int32::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (int32_t value : value_) {
        result.push_back(std::to_string(value));
    }
    return result;
}

int64_t Int32::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes Int32::GetMin() const {
    return static_cast<int64_t>(*std::min_element(value_.begin(), value_.end()));
}

CellTypes Int32::GetMin(const std::vector<uint64_t>& mask) const {
    int32_t ans = std::numeric_limits<int32_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

CellTypes Int32::GetMax() const {
    return static_cast<int64_t>(*std::max_element(value_.begin(), value_.end()));
}

CellTypes Int32::GetMax(const std::vector<uint64_t>& mask) const {
    int32_t ans = std::numeric_limits<int32_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

bool Int32::Compare(int row, Op op, CellTypes value) const {
    int32_t lhs = value_.at(row);
    int32_t rhs = static_cast<int32_t>(std::get<int64_t>(value));
    switch (op) {
        case Op::EQ: return lhs == rhs;
        case Op::NE: return lhs != rhs;
        case Op::LT: return lhs < rhs;
        case Op::LE: return lhs <= rhs;
        case Op::GT: return lhs > rhs;
        case Op::GE: return lhs >= rhs;
    }
    return false;
}

void Int32::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes current = transform ? transform(CellTypes(static_cast<int64_t>(value_[i]))) : CellTypes(static_cast<int64_t>(value_[i]));
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

void Int32::FillHashSet(std::unordered_set<int64_t>& set) const {
    for (int32_t value : value_) {
        set.insert(static_cast<int64_t>(value));
    }
}

void Int32::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const {
    for (uint64_t id : mask) {
        set.insert(static_cast<int64_t>(value_[id]));
    }
}

void Int32::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<int32_t> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void Int32::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(int32_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

void Int64::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(int64_t) * value_.size());
}

void Int64::AddCell(const std::string& cell) {
    try {
        int64_t num = std::stoll(cell);
        value_.push_back(num);
    } catch (...) {
        value_.push_back(0);
    }
}

void Int64::AddColumn(const std::vector<std::string>& col) {
    for (auto cell : col) {
        try {
            int64_t num = std::stoll(cell);
            value_.push_back(num);
        } catch (...) {
            value_.push_back(0);
        }
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

int64_t Int64::GetSum(const std::function<int64_t(int64_t)>& transform) const {
    int64_t ans = 0;
    for (auto el : value_) {
        ans += transform ? transform(el) : el;
    }
    return ans;
}

CellTypes Int64::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return *it;
}

CellTypes Int64::GetMax(const std::function<CellTypes(int64_t)>& transform) const {
    CellTypes max_val = transform(value_.front());
    for (size_t i = 1; i < value_.size(); ++i) {
        CellTypes current = transform(value_[i]);
        if (current > max_val) {
            max_val = std::move(current);
        }
    }
    return max_val;
}

CellTypes Int64::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
}

CellTypes Int64::GetMin(const std::function<CellTypes(int64_t)>& transform) const {
    CellTypes min_val = transform(value_.front());
    for (size_t i = 1; i < value_.size(); ++i) {
        CellTypes current = transform(value_[i]);
        if (current < min_val) {
            min_val = std::move(current);
        }
    }
    return min_val;
}

void Int64::FillHashSet(std::unordered_set<int64_t>& set) const {
    for (const auto& el : value_) {
        set.insert(el);
    }
}

void Int64::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const {
    for (auto id : mask) {
        set.insert(value_[id]);
    }
}

void Int64::FillHashSet(std::unordered_set<int64_t>& set, const std::function<int64_t(int64_t)>& transform) const {
    for (const auto& el : value_) {
        set.insert(transform(el));
    }
}

void Int64::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask, const std::function<int64_t(int64_t)>& transform) const {
    for (auto id : mask) {
        set.insert(transform(value_[id]));
    }
}

void Int64::AddCell(const CellTypes& cell) {
    int64_t val = std::get<int64_t>(cell);
    value_.push_back(val);
}



void Int64::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes current = transform ? transform(CellTypes(value_[i])) : CellTypes(value_[i]);
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

int64_t Int64::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

int64_t Int64::GetSum(const std::vector<uint64_t>& mask, const std::function<int64_t(int64_t)>& transform) const {
    int64_t ans = 0;
    for (uint64_t id : mask) {
        ans += transform ? transform(value_[id]) : value_[id];
    }
    return ans;
}

CellTypes Int64::GetMin(const std::vector<uint64_t>& mask) const {
    int64_t ans = std::numeric_limits<int64_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return ans;
}

CellTypes Int64::GetMin(const std::vector<uint64_t>& mask, const std::function<CellTypes(int64_t)>& transform) const {
    CellTypes min_val = transform(value_[mask.front()]);
    for (size_t i = 1; i < mask.size(); ++i) {
        CellTypes current = transform(value_[mask[i]]);
        if (current < min_val) {
            min_val = std::move(current);
        }
    }
    return min_val;
}

CellTypes Int64::GetMax(const std::vector<uint64_t>& mask) const {
    int64_t ans = std::numeric_limits<int64_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return ans;
}

CellTypes Int64::GetMax(const std::vector<uint64_t>& mask, const std::function<CellTypes(int64_t)>& transform) const {
    CellTypes max_val = transform(value_[mask.front()]);
    for (size_t i = 1; i < mask.size(); ++i) {
        CellTypes current = transform(value_[mask[i]]);
        if (current > max_val) {
            max_val = std::move(current);
        }
    }
    return max_val;
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

void String::AddColumn(const std::vector<std::string>& col) {
    for (auto cell : col) {
        try {
            value_.emplace_back(cell);
            size_ += sizeof(int64_t) + cell.size();
        } catch (...) {
            throw std::runtime_error("Cannot add cell to String column.");
        }
    }
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

CellTypes String::GetMax(const std::function<CellTypes(const std::string&)>& transform) const {
    CellTypes max_val = transform(value_.front());
    for (size_t i = 1; i < value_.size(); ++i) {
        CellTypes current = transform(value_[i]);
        if (current > max_val) {
            max_val = std::move(current);
        }
    }
    return max_val;
}

CellTypes String::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
}

CellTypes String::GetMin(const std::function<CellTypes(const std::string&)>& transform) const {
    CellTypes min_val = transform(value_.front());
    for (size_t i = 1; i < value_.size(); ++i) {
        CellTypes current = transform(value_[i]);
        if (current < min_val) {
            min_val = std::move(current);
        }
    }
    return min_val;
}

void String::FillHashSet(std::unordered_set<std::string>& set) const {
    for (const auto& el : value_) {
        set.insert(el);
    }
}

void String::FillHashSet(std::unordered_set<std::string>& set, const std::vector<uint64_t>& mask) const {
    for (auto id : mask) {
        set.insert(value_[id]);
    }
}

void String::FillHashSet(std::unordered_set<int64_t>& set, const std::function<int64_t(const std::string&)>& transform) const {
    for (const auto& el : value_) {
        set.insert(transform(el));
    }
}

void String::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask, const std::function<int64_t(const std::string&)>& transform) const {
    for (auto id : mask) {
        set.insert(transform(value_[id]));
    }
}

void String::FillHashSet(std::unordered_set<std::string>& set, const std::function<std::string(const std::string&)>& transform) const {
    for (const auto& el : value_) {
        set.insert(transform(el));
    }
}

void String::FillHashSet(std::unordered_set<std::string>& set, const std::vector<uint64_t>& mask, const std::function<std::string(const std::string&)>& transform) const {
    for (auto id : mask) {
        set.insert(transform(value_[id]));
    }
}

void String::AddCell(const CellTypes& cell) {
    std::string val = std::get<std::string>(cell);
    value_.emplace_back(val);
}

void String::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes current = transform ? transform(CellTypes(value_[i])) : CellTypes(value_[i]);
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

int64_t String::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes String::GetMin(const std::vector<uint64_t>& mask) const {
    if (mask.empty()) {
        return std::string("");
    }
    std::string_view min_val = value_[mask[0]];
    for (size_t i = 1; i < mask.size(); ++i) {
        auto id = mask[i];
        if (value_[id] < min_val) {
            min_val = value_[id];
        }
    }
    return std::string(min_val);
}

CellTypes String::GetMin(const std::vector<uint64_t>& mask, const std::function<CellTypes(const std::string&)>& transform) const {
    CellTypes min_val = transform(value_[mask.front()]);
    for (size_t i = 1; i < mask.size(); ++i) {
        CellTypes current = transform(value_[mask[i]]);
        if (current < min_val) {
            min_val = std::move(current);
        }
    }
    return min_val;
}

CellTypes String::GetMax(const std::vector<uint64_t>& mask) const {
    if (mask.empty()) {
        return std::string("");
    }
    std::string_view max_val = value_[mask[0]];
    for (size_t i = 1; i < mask.size(); ++i) {
        auto id = mask[i];
        if (value_[id] > max_val) {
            max_val = value_[id];
        }
    }
    return std::string(max_val);
}

CellTypes String::GetMax(const std::vector<uint64_t>& mask, const std::function<CellTypes(const std::string&)>& transform) const {
    CellTypes max_val = transform(value_[mask.front()]);
    for (size_t i = 1; i < mask.size(); ++i) {
        CellTypes current = transform(value_[mask[i]]);
        if (current > max_val) {
            max_val = std::move(current);
        }
    }
    return max_val;
}

Double::Double(double value) {
    value_.push_back(value);
}

void Double::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(double) * value_.size());
}

void Double::AddCell(const std::string& cell) {
    try {
        value_.push_back(std::stod(cell));
    } catch (...) {
        value_.push_back(0.0);
    }
}

void Double::AddColumn(const std::vector<std::string>& col) {
    for (const auto& cell : col) {
        AddCell(cell);
    }
}

int64_t Double::GetLastCellSize() const {
    return sizeof(double);
}

size_t Double::GetColumnByteSize() const {
    return value_.size() * sizeof(double);
}

std::string Double::GetCellAsString(int64_t i) const {
    return std::to_string(value_[i]);
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
    return value_.size();
}

int64_t Double::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

double Double::GetSum(const std::function<double(double)>& transform) const {
    double ans = 0.0;
    for (double el : value_) {
        ans += transform ? transform(el) : el;
    }
    return ans;
}

double Double::GetSum(const std::vector<uint64_t>& mask, const std::function<double(double)>& transform) const {
    double ans = 0.0;
    for (uint64_t id : mask) {
        ans += transform ? transform(value_[id]) : value_[id];
    }
    return ans;
}

CellTypes Double::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
}

CellTypes Double::GetMin(const std::vector<uint64_t>& mask) const {
    double ans = std::numeric_limits<double>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return ans;
}

CellTypes Double::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return *it;
}

CellTypes Double::GetMax(const std::vector<uint64_t>& mask) const {
    double ans = std::numeric_limits<double>::lowest();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return ans;
}

bool Double::Compare(int row, Op op, CellTypes val) const {
    double lhs = value_.at(row);
    double rhs = std::get<double>(val);
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

void Double::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<double> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void Double::Clear() {
    value_.clear();
}

void Double::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(double);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

void Double::AddCell(const CellTypes& cell) {
    double val = std::get<double>(cell);
    value_.push_back(val);
}

void Double::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes current = transform ? transform(CellTypes(value_[i])) : CellTypes(value_[i]);
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

void DateTime::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(uint32_t) * value_.size());
}

void DateTime::AddCell(const std::string& cell) {
    try {
        value_.push_back(ParseDateTime(cell));
    } catch (...) {
        value_.push_back(0);
    }
}

void DateTime::AddCell(const CellTypes& cell) {
    if (std::holds_alternative<std::string>(cell)) {
        AddCell(std::get<std::string>(cell));
        return;
    }
    value_.push_back(static_cast<uint32_t>(std::get<int64_t>(cell)));
}

void DateTime::AddColumn(const std::vector<std::string>& col) {
    for (const auto& cell : col) {
        AddCell(cell);
    }
}

size_t DateTime::GetColumnByteSize() const {
    return value_.size() * sizeof(uint32_t);
}

std::string DateTime::GetCellAsString(int64_t i) const {
    return FormatDateTime(value_[i]);
}

std::vector<std::string> DateTime::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (int64_t i = 0; i < value_.size(); ++i) {
        result.push_back(GetCellAsString(i));
    }
    return result;
}

int64_t DateTime::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes DateTime::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return FormatDateTime(*it);
}

CellTypes DateTime::GetMin(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return FormatDateTime(ans);
}

CellTypes DateTime::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return FormatDateTime(*it);
}

CellTypes DateTime::GetMax(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return FormatDateTime(ans);
}

bool DateTime::Compare(int row, Op op, CellTypes val) const {
    uint32_t lhs = value_.at(row);
    uint32_t rhs = 0;
    if (std::holds_alternative<std::string>(val)) {
        rhs = ParseDateTime(std::get<std::string>(val));
    } else {
        rhs = static_cast<uint32_t>(std::get<int64_t>(val));
    }
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

void DateTime::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes source = GetCellAsString(i);
        CellTypes current = transform ? transform(source) : source;
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

void DateTime::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<uint32_t> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void DateTime::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(uint32_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}

void Timestamp::Write(std::ostream& output) {
    output.write(reinterpret_cast<const char*>(value_.data()), sizeof(uint32_t) * value_.size());
}

void Timestamp::AddCell(const std::string& cell) {
    try {
        value_.push_back(static_cast<uint32_t>(std::stoul(cell)));
    } catch (...) {
        value_.push_back(0);
    }
}

void Timestamp::AddCell(const CellTypes& cell) {
    if (std::holds_alternative<std::string>(cell)) {
        AddCell(std::get<std::string>(cell));
        return;
    }
    value_.push_back(static_cast<uint32_t>(std::get<int64_t>(cell)));
}

void Timestamp::AddColumn(const std::vector<std::string>& col) {
    for (const auto& cell : col) {
        AddCell(cell);
    }
}

size_t Timestamp::GetColumnByteSize() const {
    return value_.size() * sizeof(uint32_t);
}

std::string Timestamp::GetCellAsString(int64_t i) const {
    return std::to_string(value_[i]);
}

std::vector<std::string> Timestamp::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (uint32_t value : value_) {
        result.push_back(std::to_string(value));
    }
    return result;
}

int64_t Timestamp::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes Timestamp::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return static_cast<int64_t>(*it);
}

CellTypes Timestamp::GetMin(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

CellTypes Timestamp::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return static_cast<int64_t>(*it);
}

CellTypes Timestamp::GetMax(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return static_cast<int64_t>(ans);
}

bool Timestamp::Compare(int row, Op op, CellTypes val) const {
    uint32_t lhs = value_.at(row);
    uint32_t rhs = 0;
    if (std::holds_alternative<std::string>(val)) {
        rhs = static_cast<uint32_t>(std::stoul(std::get<std::string>(val)));
    } else {
        rhs = static_cast<uint32_t>(std::get<int64_t>(val));
    }
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

void Timestamp::MergeHashes(
    std::vector<uint64_t>& hashes,
    std::vector<std::vector<std::string>>& group_name,
    const std::function<CellTypes(const CellTypes&)>& transform
) const {
    for (int64_t i = 0; i < value_.size(); ++i) {
        CellTypes source = static_cast<int64_t>(value_[i]);
        CellTypes current = transform ? transform(source) : source;
        hashes[i] = HashCombine(hashes[i], HashCell(current));
        if (!group_name.empty()) {
            group_name[i].push_back(CellToString(current));
        }
    }
}

void Timestamp::FillHashSet(std::unordered_set<int64_t>& set) const {
    for (uint32_t value : value_) {
        set.insert(static_cast<int64_t>(value));
    }
}

void Timestamp::FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const {
    for (uint64_t id : mask) {
        set.insert(static_cast<int64_t>(value_[id]));
    }
}

void Timestamp::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<uint32_t> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void Timestamp::SetData(const std::vector<uint8_t>& data) {
    int64_t count = data.size() / sizeof(uint32_t);
    value_.resize(count);
    std::memcpy(value_.data(), data.data(), data.size());
}
