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
