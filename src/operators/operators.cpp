#include "operators.h"

std::optional<Batch> ScanOperator::Next() {
    std::vector<int> ids;
    for (const auto& name : columns_) {
        ids.push_back(reader_.GetScheme().GetColumnIndex(name));
    }
    std::optional<Batch> batch = reader_.ReadNextBatch(ids);
    if (!batch.has_value()) {
        return std::nullopt;
    }
    return std::move(batch);
}