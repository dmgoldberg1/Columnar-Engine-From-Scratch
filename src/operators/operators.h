#pragma once

#include "../column_types/column_types.h"
#include "../file_reader/file_reader.h"

#include <optional>

using Batch = std::vector<std::unique_ptr<Column>>;

class IOperator {
public:
    virtual std::optional<Batch> Next() = 0;
    virtual ~IOperator() = default;
};

class ScanOperator : public IOperator {
public:
    ScanOperator(const std::string& filename, const std::vector<std::string>& columns) 
        : columns_(columns),
          file_(filename, std::ios::binary | std::ios::ate),
          reader_(file_) {}
          
    std::optional<Batch> Next() override;
protected:
    std::vector<std::string> columns_;
    std::ifstream file_;
    RowGroupReader reader_;
    int curr_batch_ = 0;
};

class FilterOperator : public IOperator {

};