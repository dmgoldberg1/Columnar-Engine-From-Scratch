#pragma once

#include <fstream>
#include <string>
#include <vector>
#include <variant>
#include <memory>


class Column {
public:
    virtual void Write(std::ostream& output) = 0;
    virtual void AddCell(const std::string& cell) = 0;
    virtual int64_t GetLastCellSize() const = 0;
    virtual size_t GetColumnByteSize() const = 0;
    virtual std::string GetCellAsString(int64_t i) const = 0;
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
    int64_t GetLastCellSize() const override {
        return sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override;
    std::string GetCellAsString(int64_t i) const override { return std::to_string(value_[i]); }
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
    int64_t GetLastCellSize() const override {
        return value_.back().size() + sizeof(int64_t);
    }
    size_t GetColumnByteSize() const override { return size_; }
    std::string GetCellAsString(int64_t i) const override { return value_[i]; }
    void Clear() override {
        value_.clear();
        size_ = 0;
    }

    void SetData(const std::vector<uint8_t>& data) override;
protected:
    std::vector<std::string> value_;
    size_t size_ = 0;
};