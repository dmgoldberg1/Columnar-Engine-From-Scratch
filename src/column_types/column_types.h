#pragma once

#include <fstream>
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

    virtual int64_t GetLastCellSize() const = 0;
    virtual size_t GetColumnByteSize() const = 0;
    virtual std::string GetCellAsString(int64_t i) const = 0;
    virtual std::vector<std::string> GetColumnAsString() const = 0;
    virtual int64_t GetRowCount() const = 0;
    virtual CellTypes GetMin() const = 0;
    virtual CellTypes GetMax() const = 0;

    enum class Op { EQ, NE, LT, LE, GT, GE };
    virtual bool Compare(int row, Op op, CellTypes val) const = 0;

    virtual void FilterRows(const std::vector<int64_t>& mask) = 0;
    virtual void Clear() = 0;
    virtual void SetData(const std::vector<uint8_t>& data) = 0;
    virtual ~Column() = default;
};

class Int64 : public Column {
public:
    Int64(int64_t value) { value_.push_back(value); }
    Int64() = default;
    void Write(std::ostream& output) override;
    void AddCell(const std::string& cell) override;
    void AddCell(const CellTypes& cell) override;
    int64_t GetLastCellSize() const override {
        return sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override { return std::to_string(value_[i]); }
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override {
        return value_.size();
    }
    int64_t GetSum() const;
    CellTypes GetMin() const override;
    CellTypes GetMax() const override;
    void FillHashSet(std::unordered_set<int64_t>& set) const;
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
    int64_t GetLastCellSize() const override {
        return value_.back().size() + sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override { return size_; }
    std::string GetCellAsString(int64_t i) const override { return value_[i]; }
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override {
        return value_.size();
    }
    CellTypes GetMax() const override;
    CellTypes GetMin() const override;
    void FillHashSet(std::unordered_set<std::string>& set) const;
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

    int64_t GetLastCellSize() const override;
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override;
    std::vector<std::string> GetColumnAsString() const override;
    int64_t GetRowCount() const override;
    CellTypes GetMin() const override;
    CellTypes GetMax() const override;

    bool Compare(int row, Op op, CellTypes val) const override;

    void FilterRows(const std::vector<int64_t>& mask) override;
    void Clear() override;
    void SetData(const std::vector<uint8_t>& data) override;

protected:
    std::vector<double> value_;
};