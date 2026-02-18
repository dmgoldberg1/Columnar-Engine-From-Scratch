#pragma once

#include <memory>
#include <fstream>
#include <vector>

enum MetadataIndex {
    BatchCount = 0

};

class RowGroupReader {
public:
    RowGroupReader(std::istream& input);
    void ReadToCSV(const char* filename);
    ~RowGroupReader();

protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

class Metadata {
public:
    Metadata(std::istream& input);
    void Read();
    ~Metadata();
public:
    std::vector<int64_t> GetTypesInfo() const;
    std::vector<int64_t> GetBatchStartPos() const;
    std::vector<int64_t> GetBatchMetadata(const int64_t i) const;
    int64_t GetColumnNum() const;
protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};