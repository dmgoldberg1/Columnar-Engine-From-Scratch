#include "column_types.h"

#include "../utilities/utilities.h"

#include <algorithm>
#include <stdexcept>
#include <string_view>
#include <cstring>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace {

enum class StringEncoding : uint8_t {
    Dictionary = 0,
    DeltaLengthByteArray = 1,
};

template <typename T>
void AppendBytes(std::vector<uint8_t>& output, const T& value) {
    const auto* ptr = reinterpret_cast<const uint8_t*>(&value);
    output.insert(output.end(), ptr, ptr + sizeof(T));
}

template <typename T>
T ReadBytes(const uint8_t*& ptr) {
    T value;
    std::memcpy(&value, ptr, sizeof(T));
    ptr += sizeof(T);
    return value;
}

uint64_t ZigZagEncode(int64_t value) {
    return (static_cast<uint64_t>(value) << 1) ^ static_cast<uint64_t>(value >> 63);
}

int64_t ZigZagDecode(uint64_t value) {
    return static_cast<int64_t>((value >> 1) ^ (~(value & 1) + 1));
}

uint8_t GetBitWidth(uint64_t value) {
    if (value == 0) {
        return 0;
    }
    return static_cast<uint8_t>(64 - std::countl_zero(value));
}

std::vector<uint8_t> BitPack(const std::vector<uint64_t>& values, uint8_t bit_width) {
    std::vector<uint8_t> output;
    if (bit_width == 0 || values.empty()) {
        return output;
    }
    output.reserve((values.size() * bit_width + 7) / 8);
    uint64_t buffer = 0;
    uint8_t bits_in_buffer = 0;
    for (uint64_t value : values) {
        buffer |= (value << bits_in_buffer);
        bits_in_buffer += bit_width;
        while (bits_in_buffer >= 8) {
            output.push_back(static_cast<uint8_t>(buffer & 0xFF));
            buffer >>= 8;
            bits_in_buffer -= 8;
        }
    }
    if (bits_in_buffer > 0) {
        output.push_back(static_cast<uint8_t>(buffer & 0xFF));
    }
    return output;
}

std::vector<uint64_t> BitUnpack(const uint8_t*& ptr, size_t count, uint8_t bit_width) {
    std::vector<uint64_t> output(count, 0);
    if (bit_width == 0 || count == 0) {
        return output;
    }
    uint64_t buffer = 0;
    uint8_t bits_in_buffer = 0;
    const uint64_t mask = bit_width == 64 ? std::numeric_limits<uint64_t>::max() : ((1ULL << bit_width) - 1);
    for (size_t i = 0; i < count; ++i) {
        while (bits_in_buffer < bit_width) {
            buffer |= (static_cast<uint64_t>(*ptr++) << bits_in_buffer);
            bits_in_buffer += 8;
        }
        output[i] = buffer & mask;
        buffer >>= bit_width;
        bits_in_buffer -= bit_width;
    }
    return output;
}

template <typename T>
std::vector<uint8_t> EncodeMinBitPacked(const std::vector<T>& values) {
    std::vector<uint8_t> output;
    output.reserve(sizeof(uint32_t) + sizeof(int64_t) + sizeof(uint8_t) + values.size() * sizeof(T));
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(values.size()));
    if (values.empty()) {
        return output;
    }
    int64_t min_value = static_cast<int64_t>(*std::min_element(values.begin(), values.end()));
    AppendBytes<int64_t>(output, min_value);
    std::vector<uint64_t> offsets;
    offsets.reserve(values.size());
    uint64_t max_offset = 0;
    for (T value : values) {
        uint64_t offset = static_cast<uint64_t>(static_cast<int64_t>(value) - min_value);
        offsets.push_back(offset);
        max_offset = std::max(max_offset, offset);
    }
    uint8_t bit_width = GetBitWidth(max_offset);
    output.push_back(bit_width);
    std::vector<uint8_t> packed = BitPack(offsets, bit_width);
    output.insert(output.end(), packed.begin(), packed.end());
    return output;
}

template <typename T>
void DecodeMinBitPacked(const std::vector<uint8_t>& data, std::vector<T>& values) {
    const uint8_t* ptr = data.data();
    uint32_t count = ReadBytes<uint32_t>(ptr);
    values.resize(count);
    if (count == 0) {
        return;
    }
    int64_t min_value = ReadBytes<int64_t>(ptr);
    uint8_t bit_width = *ptr++;
    std::vector<uint64_t> offsets = BitUnpack(ptr, count, bit_width);
    for (uint32_t i = 0; i < count; ++i) {
        values[i] = static_cast<T>(min_value + static_cast<int64_t>(offsets[i]));
    }
}

template <typename T>
std::vector<uint8_t> EncodeDeltaBitPacked(const std::vector<T>& values) {
    std::vector<uint8_t> output;
    output.reserve(sizeof(uint32_t) + sizeof(int64_t) + sizeof(uint8_t) + values.size() * sizeof(T));
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(values.size()));
    if (values.empty()) {
        return output;
    }
    AppendBytes<int64_t>(output, static_cast<int64_t>(values.front()));
    if (values.size() == 1) {
        output.push_back(0);
        return output;
    }
    std::vector<uint64_t> deltas;
    deltas.reserve(values.size() - 1);
    uint64_t max_delta = 0;
    int64_t previous = static_cast<int64_t>(values.front());
    for (size_t i = 1; i < values.size(); ++i) {
        int64_t current = static_cast<int64_t>(values[i]);
        uint64_t encoded_delta = ZigZagEncode(current - previous);
        deltas.push_back(encoded_delta);
        max_delta = std::max(max_delta, encoded_delta);
        previous = current;
    }
    uint8_t bit_width = GetBitWidth(max_delta);
    output.push_back(bit_width);
    std::vector<uint8_t> packed = BitPack(deltas, bit_width);
    output.insert(output.end(), packed.begin(), packed.end());
    return output;
}

template <typename T>
void DecodeDeltaBitPacked(const std::vector<uint8_t>& data, std::vector<T>& values) {
    const uint8_t* ptr = data.data();
    uint32_t count = ReadBytes<uint32_t>(ptr);
    values.resize(count);
    if (count == 0) {
        return;
    }
    int64_t first_value = ReadBytes<int64_t>(ptr);
    values[0] = static_cast<T>(first_value);
    if (count == 1) {
        return;
    }
    uint8_t bit_width = *ptr++;
    std::vector<uint64_t> deltas = BitUnpack(ptr, count - 1, bit_width);
    int64_t current = first_value;
    for (uint32_t i = 1; i < count; ++i) {
        current += ZigZagDecode(deltas[i - 1]);
        values[i] = static_cast<T>(current);
    }
}

std::vector<uint8_t> EncodeStringDictionary(const std::vector<std::string>& values) {
    std::vector<uint8_t> output;
    std::unordered_map<std::string, uint32_t> dictionary_ids;
    dictionary_ids.reserve(values.size());
    std::vector<std::string> dictionary;
    std::vector<uint64_t> ids;
    dictionary.reserve(values.size());
    ids.reserve(values.size());
    size_t dictionary_bytes = 0;
    for (const auto& value : values) {
        auto [it, inserted] = dictionary_ids.emplace(value, static_cast<uint32_t>(dictionary.size()));
        if (inserted) {
            dictionary.push_back(value);
            dictionary_bytes += value.size();
        }
        ids.push_back(it->second);
    }
    size_t packed_bytes = dictionary.empty() ? 0 : (values.size() * GetBitWidth(dictionary.size() - 1) + 7) / 8;
    output.reserve(sizeof(uint32_t) + dictionary.size() * sizeof(uint32_t) + dictionary_bytes + sizeof(uint8_t) + packed_bytes);
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(dictionary.size()));
    for (const auto& entry : dictionary) {
        AppendBytes<uint32_t>(output, static_cast<uint32_t>(entry.size()));
        output.insert(output.end(), entry.begin(), entry.end());
    }
    uint8_t bit_width = GetBitWidth(dictionary.empty() ? 0 : dictionary.size() - 1);
    output.push_back(bit_width);
    std::vector<uint8_t> packed = BitPack(ids, bit_width);
    output.insert(output.end(), packed.begin(), packed.end());
    return output;
}

void DecodeStringDictionary(const uint8_t*& ptr, uint32_t count, std::vector<std::string>& values) {
    uint32_t dictionary_size = ReadBytes<uint32_t>(ptr);
    std::vector<std::string> dictionary;
    dictionary.reserve(dictionary_size);
    for (uint32_t i = 0; i < dictionary_size; ++i) {
        uint32_t length = ReadBytes<uint32_t>(ptr);
        dictionary.emplace_back(reinterpret_cast<const char*>(ptr), length);
        ptr += length;
    }
    uint8_t bit_width = *ptr++;
    std::vector<uint64_t> ids = BitUnpack(ptr, count, bit_width);
    for (uint32_t i = 0; i < count; ++i) {
        values[i] = dictionary[ids[i]];
    }
}

bool ShouldUseDictionaryEncoding(const std::vector<std::string>& values) {
    constexpr size_t kSampleSize = 4096;
    constexpr double kDistinctRatioThreshold = 0.70;
    if (values.size() < 128) {
        return true;
    }
    size_t sample_size = std::min(values.size(), kSampleSize);
    std::unordered_set<std::string_view> distinct;
    distinct.reserve(sample_size);
    for (size_t i = 0; i < sample_size; ++i) {
        distinct.insert(values[i]);
    }
    double distinct_ratio = static_cast<double>(distinct.size()) / static_cast<double>(sample_size);
    return distinct_ratio <= kDistinctRatioThreshold;
}

std::vector<uint8_t> EncodeStringDeltaLengthByteArray(const std::vector<std::string>& values) {
    std::vector<uint8_t> output;
    if (values.empty()) {
        return output;
    }
    size_t total_bytes = 0;
    for (const auto& value : values) {
        total_bytes += value.size();
    }
    output.reserve(sizeof(uint32_t) + total_bytes + (values.size() - 1) * sizeof(uint32_t) * 2);
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(values.front().size()));
    output.insert(output.end(), values.front().begin(), values.front().end());
    for (size_t i = 1; i < values.size(); ++i) {
        const std::string& previous = values[i - 1];
        const std::string& current = values[i];
        uint32_t prefix_len = 0;
        uint32_t common_limit = static_cast<uint32_t>(std::min(previous.size(), current.size()));
        while (prefix_len < common_limit && previous[prefix_len] == current[prefix_len]) {
            ++prefix_len;
        }
        uint32_t suffix_len = static_cast<uint32_t>(current.size()) - prefix_len;
        AppendBytes<uint32_t>(output, prefix_len);
        AppendBytes<uint32_t>(output, suffix_len);
        output.insert(output.end(), current.begin() + prefix_len, current.end());
    }
    return output;
}

void DecodeStringDeltaLengthByteArray(const uint8_t*& ptr, uint32_t count, std::vector<std::string>& values) {
    if (count == 0) {
        return;
    }
    uint32_t first_length = ReadBytes<uint32_t>(ptr);
    values[0] = std::string(reinterpret_cast<const char*>(ptr), first_length);
    ptr += first_length;
    for (uint32_t i = 1; i < count; ++i) {
        uint32_t prefix_len = ReadBytes<uint32_t>(ptr);
        uint32_t suffix_len = ReadBytes<uint32_t>(ptr);
        std::string current;
        current.reserve(prefix_len + suffix_len);
        current.append(values[i - 1].data(), prefix_len);
        current.append(reinterpret_cast<const char*>(ptr), suffix_len);
        ptr += suffix_len;
        values[i] = std::move(current);
    }
}

std::vector<uint8_t> EncodeStringColumn(const std::vector<std::string>& values) {
    std::vector<uint8_t> output;
    output.reserve(sizeof(uint32_t) + sizeof(uint8_t));
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(values.size()));
    if (values.empty()) {
        return output;
    }
    if (ShouldUseDictionaryEncoding(values)) {
        output.push_back(static_cast<uint8_t>(StringEncoding::Dictionary));
        std::vector<uint8_t> payload = EncodeStringDictionary(values);
        output.insert(output.end(), payload.begin(), payload.end());
    } else {
        output.push_back(static_cast<uint8_t>(StringEncoding::DeltaLengthByteArray));
        std::vector<uint8_t> payload = EncodeStringDeltaLengthByteArray(values);
        output.insert(output.end(), payload.begin(), payload.end());
    }
    return output;
}

void DecodeStringColumn(const std::vector<uint8_t>& data, std::vector<std::string>& values) {
    const uint8_t* ptr = data.data();
    uint32_t count = ReadBytes<uint32_t>(ptr);
    values.resize(count);
    if (count == 0) {
        return;
    }
    StringEncoding encoding = static_cast<StringEncoding>(*ptr++);
    if (encoding == StringEncoding::Dictionary) {
        DecodeStringDictionary(ptr, count, values);
    } else {
        DecodeStringDeltaLengthByteArray(ptr, count, values);
    }
}

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

std::vector<uint8_t> Int16::Encode() const {
    return EncodeMinBitPacked(value_);
}

void Int16::Decode(const std::vector<uint8_t>& data) {
    DecodeMinBitPacked(data, value_);
}

void Int16::AddCell(const std::string& cell) {
    value_.push_back(static_cast<int16_t>(std::stoi(cell)));
}

void Int16::AddCell(const CellTypes& cell) {
    value_.push_back(static_cast<int16_t>(std::get<int64_t>(cell)));
}

void Int16::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        value_.push_back(static_cast<int16_t>(std::stoi(cell)));
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
    Decode(data);
}

std::vector<uint8_t> Int32::Encode() const {
    return EncodeMinBitPacked(value_);
}

void Int32::Decode(const std::vector<uint8_t>& data) {
    DecodeMinBitPacked(data, value_);
}

void Int32::AddCell(const std::string& cell) {
    value_.push_back(std::stoi(cell));
}

void Int32::AddCell(const CellTypes& cell) {
    value_.push_back(static_cast<int32_t>(std::get<int64_t>(cell)));
}

void Int32::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        value_.push_back(std::stoi(cell));
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
    Decode(data);
}

std::vector<uint8_t> Int64::Encode() const {
    return EncodeDeltaBitPacked(value_);
}

void Int64::Decode(const std::vector<uint8_t>& data) {
    DecodeDeltaBitPacked(data, value_);
}

void Int64::AddCell(const std::string& cell) {
    value_.push_back(std::stoll(cell));
}

void Int64::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        value_.push_back(std::stoll(cell));
    }
}

void Int64::SetData(const std::vector<uint8_t>& data) {
    Decode(data);
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

CellTypes Int64::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return *it;
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

CellTypes Int64::GetMax(const std::vector<uint64_t>& mask) const {
    int64_t ans = std::numeric_limits<int64_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return ans;
}

std::vector<uint8_t> String::Encode() const {
    return EncodeStringColumn(value_);
}

void String::Decode(const std::vector<uint8_t>& data) {
    DecodeStringColumn(data, value_);
    size_ = 0;
    for (const auto& value : value_) {
        size_ += sizeof(int64_t) + value.size();
    }
}

void String::AddCell(const std::string& cell) {
    size_ += sizeof(int64_t) + cell.size();
    value_.push_back(std::move(cell));
    
}

void String::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        try {
            value_.emplace_back(cell);
            size_ += sizeof(int64_t) + cell.size();
        } catch (...) {
            throw std::runtime_error("Cannot add cell to String column.");
        }
    }
}

void String::SetData(const std::vector<uint8_t>& data) {
    Decode(data);
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

void String::FillHashSet(std::unordered_set<std::string>& set, const std::vector<uint64_t>& mask) const {
    for (auto id : mask) {
        set.insert(value_[id]);
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

Double::Double(double value) {
    value_.push_back(value);
}

std::vector<uint8_t> Double::Encode() const {
    std::vector<uint8_t> output;
    output.reserve(sizeof(uint32_t) + value_.size() * sizeof(double));
    AppendBytes<uint32_t>(output, static_cast<uint32_t>(value_.size()));
    for (double value : value_) {
        AppendBytes<double>(output, value);
    }
    return output;
}

void Double::Decode(const std::vector<uint8_t>& data) {
    const uint8_t* ptr = data.data();
    uint32_t count = ReadBytes<uint32_t>(ptr);
    value_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        value_[i] = ReadBytes<double>(ptr);
    }
}

void Double::AddCell(const std::string& cell) {
    try {
        value_.push_back(std::stod(cell));
    } catch (...) {
        value_.push_back(0.0);
    }
}

void Double::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        try {
            value_.push_back(std::stod(cell));
        } catch (...) {
            value_.push_back(0.0);
        }
    }
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
    Decode(data);
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

std::vector<uint8_t> Date::Encode() const {
    return EncodeDeltaBitPacked(value_);
}

void Date::Decode(const std::vector<uint8_t>& data) {
    DecodeDeltaBitPacked(data, value_);
}

void Date::AddCell(const std::string& cell) {
    try {
        value_.push_back(ParseDate(cell));
    } catch (...) {
        value_.push_back(0);
    }
}

void Date::AddCell(const CellTypes& cell) {
    if (std::holds_alternative<std::string>(cell)) {
        AddCell(std::get<std::string>(cell));
        return;
    }
    value_.push_back(static_cast<uint32_t>(std::get<int64_t>(cell)));
}

void Date::AddColumn(const std::vector<std::string>& col) {
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        try {
            value_.push_back(ParseDate(cell));
        } catch (...) {
            value_.push_back(0);
        }
    }
}

size_t Date::GetColumnByteSize() const {
    return value_.size() * sizeof(uint32_t);
}

std::string Date::GetCellAsString(int64_t i) const {
    return FormatDate(value_[i]);
}

std::vector<std::string> Date::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (int64_t i = 0; i < value_.size(); ++i) {
        result.push_back(GetCellAsString(i));
    }
    return result;
}

int64_t Date::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes Date::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return FormatDate(*it);
}

CellTypes Date::GetMin(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return FormatDate(ans);
}

CellTypes Date::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return FormatDate(*it);
}

CellTypes Date::GetMax(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return FormatDate(ans);
}

bool Date::Compare(int row, Op op, CellTypes val) const {
    uint32_t lhs = value_.at(row);
    uint32_t rhs = 0;
    if (std::holds_alternative<std::string>(val)) {
        rhs = ParseDate(std::get<std::string>(val));
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

void Date::MergeHashes(
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

void Date::FilterRows(const std::vector<int64_t>& mask) {
    std::vector<uint32_t> new_values;
    new_values.reserve(mask.size());
    for (int64_t id : mask) {
        new_values.push_back(value_[id]);
    }
    value_ = std::move(new_values);
}

void Date::SetData(const std::vector<uint8_t>& data) {
    Decode(data);
}

std::vector<uint8_t> Timestamp::Encode() const {
    return EncodeDeltaBitPacked(value_);
}

void Timestamp::Decode(const std::vector<uint8_t>& data) {
    DecodeDeltaBitPacked(data, value_);
}

void Timestamp::AddCell(const std::string& cell) {
    try {
        value_.push_back(ParseTimestamp(cell));
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
    value_.reserve(value_.size() + col.size());
    for (const auto& cell : col) {
        try {
            value_.push_back(ParseTimestamp(cell));
        } catch (...) {
            value_.push_back(0);
        }
    }
}

size_t Timestamp::GetColumnByteSize() const {
    return value_.size() * sizeof(uint32_t);
}

std::string Timestamp::GetCellAsString(int64_t i) const {
    return FormatTimestamp(value_[i]);
}

std::vector<std::string> Timestamp::GetColumnAsString() const {
    std::vector<std::string> result;
    result.reserve(value_.size());
    for (uint32_t value : value_) {
        result.push_back(FormatTimestamp(value));
    }
    return result;
}

int64_t Timestamp::GetRowCount(const std::vector<uint64_t>& mask) const {
    return mask.size();
}

CellTypes Timestamp::GetMin() const {
    auto it = std::min_element(value_.begin(), value_.end());
    return FormatTimestamp(*it);
}

CellTypes Timestamp::GetMin(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::max();
    for (auto id : mask) {
        ans = std::min(ans, value_[id]);
    }
    return FormatTimestamp(ans);
}

CellTypes Timestamp::GetMax() const {
    auto it = std::max_element(value_.begin(), value_.end());
    return FormatTimestamp(*it);
}

CellTypes Timestamp::GetMax(const std::vector<uint64_t>& mask) const {
    uint32_t ans = std::numeric_limits<uint32_t>::min();
    for (auto id : mask) {
        ans = std::max(ans, value_[id]);
    }
    return FormatTimestamp(ans);
}

bool Timestamp::Compare(int row, Op op, CellTypes val) const {
    uint32_t lhs = value_.at(row);
    uint32_t rhs = 0;
    if (std::holds_alternative<std::string>(val)) {
        rhs = ParseTimestamp(std::get<std::string>(val));
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
        CellTypes source = GetCellAsString(i);
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
    Decode(data);
}
