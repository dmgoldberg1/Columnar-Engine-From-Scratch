#pragma once

#include "../column_types/column_types.h"
#include "../scheme/scheme.h"

#include <memory>
#include <fstream>
#include <vector>
#include <optional>

using Batch = std::vector<std::unique_ptr<Column>>;

enum MetadataIndex {
    BatchCount = 0

};

class RowGroupReader {
public:
    RowGroupReader(std::istream& input);
    void ReadToCSV(const char* filename);
    std::optional<Batch> ReadNextBatch(const std::vector<int>& ids);
    Scheme GetScheme() const;
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
    Scheme GetScheme() const;
protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};