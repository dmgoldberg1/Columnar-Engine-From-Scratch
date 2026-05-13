#pragma once

#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <variant>
#include <memory>
#include <unordered_set>

using CellTypes = std::variant<int64_t, std::string, double>;

class Column {
public:
    virtual void Write(std::ostream& output) = 0;
    virtual void AddCell(const std::string& cell) = 0;
    virtual void AddCell(const CellTypes& cell) = 0;
    virtual void AddColumn(const std::vector<std::string>& col) = 0;

    virtual int64_t GetLastCellSize() const = 0;
    virtual size_t GetColumnByteSize() const = 0;
    virtual std::string GetCellAsString(int64_t i) const = 0;
    virtual std::vector<std::string> GetColumnAsString() const = 0;

    virtual int64_t GetRowCount() const = 0;
    virtual int64_t GetRowCount(const std::vector<uint64_t>& mask) const = 0;
    virtual CellTypes GetMin() const = 0;
    virtual CellTypes GetMin(const std::vector<uint64_t>& mask) const = 0;
    virtual CellTypes GetMax() const = 0;
    virtual CellTypes GetMax(const std::vector<uint64_t>& mask) const = 0;
    virtual CellTypes Get(int64_t r) const = 0;

    enum class Op { EQ, NE, LT, LE, GT, GE };
    virtual bool Compare(int row, Op op, CellTypes val) const = 0;

    virtual void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const = 0;
    virtual void FilterRows(const std::vector<int64_t>& mask) = 0;
    virtual void Clear() = 0;
    virtual void SetData(const std::vector<uint8_t>& data) = 0;
    virtual ~Column() = default;
};

class Int16 : public Column {
public:
    Int16(int16_t value) { value_.push_back(value); }
    Int16() = default;
    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    void AddColumn(const std::vector<std::string>& col) override;
    int64_t GetLastCellSize() const override { return sizeof(int16_t); }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override { return std::to_string(value_[i]); }
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override { return value_.size(); }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return static_cast<int64_t>(value_[r]); }

    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    bool Compare(int row, Op op, CellTypes value) const override;
    void Clear() override { value_.clear(); }
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<int16_t> value_;
};

class Int32 : public Column {
public:
    Int32(int32_t value) { value_.push_back(value); }
    Int32() = default;
    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    void AddColumn(const std::vector<std::string>& col) override;
    int64_t GetLastCellSize() const override { return sizeof(int32_t); }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override { return std::to_string(value_[i]); }
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override { return value_.size(); }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return static_cast<int64_t>(value_[r]); }

    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    bool Compare(int row, Op op, CellTypes value) const override;
    void Clear() override { value_.clear(); }
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<int32_t> value_;
};

class Int64 : public Column {
public:
    Int64(int64_t value) { value_.push_back(value); }
    Int64() = default;
    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    virtual void AddColumn(const std::vector<std::string>& col) override;
    int64_t GetLastCellSize() const override {
        return sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override { return std::to_string(value_[i]); }
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override {
        return value_.size();
    }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    int64_t GetSum(const std::function<int64_t(int64_t)>& transform = {}) const;
    int64_t GetSum(const std::vector<uint64_t>& mask, const std::function<int64_t(int64_t)>& transform = {}) const;
    CellTypes GetMin(const std::function<CellTypes(int64_t)>& transform) const;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask, const std::function<CellTypes(int64_t)>& transform) const;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax(const std::function<CellTypes(int64_t)>& transform) const;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask, const std::function<CellTypes(int64_t)>& transform) const;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return value_[r]; }

    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::function<int64_t(int64_t)>& transform) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask, const std::function<int64_t(int64_t)>& transform) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    bool Compare(int row, Op op, CellTypes value) const override;
    void Clear() override { value_.clear(); }
    void SetData(const std::vector<uint8_t>& data) override;
protected:
    std::vector<int64_t> value_;
};

class String : public Column {
public:
    String(const std::string& value) { value_.emplace_back(value); }
    ~String() = default;
    String() = default;
    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    virtual void AddColumn(const std::vector<std::string>& col) override;
    int64_t GetLastCellSize() const override {
        return value_.back().size() + sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override { return size_; }
    std::string GetCellAsString(int64_t i) const override { return value_[i]; }
    std::vector<std::string> GetColumnAsString() const override;

    int64_t GetRowCount() const override {
        return value_.size();
    }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax(const std::function<CellTypes(const std::string&)>& transform) const;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask, const std::function<CellTypes(const std::string&)>& transform) const;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMin(const std::function<CellTypes(const std::string&)>& transform) const;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask, const std::function<CellTypes(const std::string&)>& transform) const;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return value_[r]; }

    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<std::string>& set) const;
    void FillHashSet(std::unordered_set<std::string>& set, const std::vector<uint64_t>& mask) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::function<int64_t(const std::string&)>& transform) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask, const std::function<int64_t(const std::string&)>& transform) const;
    void FillHashSet(std::unordered_set<std::string>& set, const std::function<std::string(const std::string&)>& transform) const;
    void FillHashSet(std::unordered_set<std::string>& set, const std::vector<uint64_t>& mask, const std::function<std::string(const std::string&)>& transform) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    bool Compare(int row, Op op, CellTypes value) const override;
    void Clear() override {
        value_.clear();
        size_ = 0;
    }

    void SetData(const std::vector<uint8_t>& data) override;
protected:
    std::vector<std::string> value_;
    size_t size_ = 0;
};

class Double : public Column {
public:
    Double() = default;
    Double(double value);

    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    virtual void AddColumn(const std::vector<std::string>& col) override;

    int64_t GetLastCellSize() const override;
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override;
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override;
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    double GetSum(const std::function<double(double)>& transform = {}) const;
    double GetSum(const std::vector<uint64_t>& mask, const std::function<double(double)>& transform = {}) const;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return value_[r]; }

    bool Compare(int row, Op op, CellTypes val) const override;

    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FilterRows(const std::vector<int64_t>& mask) override;
    void Clear() override;
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<double> value_;
};

class DateTime : public Column {
public:
    DateTime() = default;
    explicit DateTime(uint32_t value) { value_.push_back(value); }

    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    void AddColumn(const std::vector<std::string>& col) override;

    int64_t GetLastCellSize() const override { return sizeof(uint32_t); }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override;
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override { return value_.size(); }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return GetCellAsString(r); }

    bool Compare(int row, Op op, CellTypes val) const override;
    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    void Clear() override { value_.clear(); }
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<uint32_t> value_;
};

class Timestamp : public Column {
public:
    Timestamp() = default;
    explicit Timestamp(uint32_t value) { value_.push_back(value); }

    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    void AddColumn(const std::vector<std::string>& col) override;

    int64_t GetLastCellSize() const override { return sizeof(uint32_t); }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override;
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override { return value_.size(); }
    int64_t GetRowCount(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMin() const override;
    CellTypes GetMin(const std::vector<uint64_t>& mask) const override;
    CellTypes GetMax() const override;
    CellTypes GetMax(const std::vector<uint64_t>& mask) const override;
    CellTypes Get(int64_t r) const override { return static_cast<int64_t>(value_[r]); }

    bool Compare(int row, Op op, CellTypes val) const override;
    void MergeHashes(
        std::vector<uint64_t>& hashes,
        std::vector<std::vector<std::string>>& group_name,
        const std::function<CellTypes(const CellTypes&)>& transform = {}
    ) const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
    void FillHashSet(std::unordered_set<int64_t>& set, const std::vector<uint64_t>& mask) const;
    void FilterRows(const std::vector<int64_t>& mask) override;
    void Clear() override { value_.clear(); }
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<uint32_t> value_;
};
