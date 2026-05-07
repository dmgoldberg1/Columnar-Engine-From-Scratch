#include "operators.h"

#include "../utilities/utilities.h"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <queue>

ScanOperator::ScanOperator(const std::string& filename, const std::vector<std::string>& columns) 
    : columns_(columns),
      file_(filename, std::ios::binary | std::ios::ate),
      reader_(file_) {
        for (const auto& name : columns_) {
            curr_ids_.push_back(reader_.GetScheme().GetColumnIndex(name));
        }
      }
std::optional<Batch> ScanOperator::Next() {
    std::optional<Batch> batch = reader_.ReadNextBatch(curr_ids_);
    if (!batch.has_value()) {
        return std::nullopt;
    }
    return std::move(batch);
}

std::optional<Batch> FilterOperator::Next() {
    std::optional<Batch> batch = child_->Next();
    if (!batch.has_value()) {
        return std::nullopt;
    }
    std::vector<int> curr_ids = child_->GetCurrColIds();
    int64_t row_count = batch.value()[curr_ids.front()]->GetRowCount();
    std::vector<int64_t> filtered_ids;
    for (int64_t i = 0; i < row_count; ++i) {
        if (condition_->Evaluate(batch.value(), i)) {
            filtered_ids.push_back(i);
        }
    }
    for (int i : curr_ids) {
        batch.value()[i]->FilterRows(filtered_ids);
    }
    return std::move(batch);
}

std::vector<int> FilterOperator::GetCurrColIds() const {
    return child_->GetCurrColIds();
}

void SumIntAccumulator::Update(const Column* column) {
    const auto* int_col = static_cast<const Int64*>(column);
    sum_ += int_col->GetSum();
}

void SumIntAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    const auto* int_col = static_cast<const Int64*>(column);
    sum_ += int_col->GetSum(mask);
}



void SumFloatAccumulator::Update(const Column* column) {

}

void SumFloatAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    
}

CellTypes AvgAccumulator::GetResult() const {
    if (count_ == 0) {
        return 0;
    }
    CellTypes sum_val = sum_accumulator_->GetResult();
    double average = std::visit([this](auto&& arg) -> double {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_arithmetic_v<T>) {
            return static_cast<double>(arg) / static_cast<double>(this->count_);
        } else {
            return 0.0; 
        }
    }, sum_val);
    return average;
}

void AvgAccumulator::Update(const Column* column) {
    sum_accumulator_->Update(column);
    count_ += column->GetRowCount();
}

void AvgAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    sum_accumulator_->Update(column, mask);
    count_ += column->GetRowCount(mask);
}

void CountAccumulator::Update(const Column* column) {
    count_ += column->GetRowCount();
}

void CountAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    count_ += column->GetRowCount(mask);
}

void MinAccumulator::Update(const Column* column) {
    CellTypes batch_min = column->GetMin();
    if (!has_data_) {
        min_ = batch_min;
        has_data_ = true;
    } else if (batch_min < min_) {
        min_ = batch_min;
    }
}

void MinAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    CellTypes batch_min = column->GetMin(mask);
    if (!has_data_) {
        min_ = batch_min;
        has_data_ = true;
    } else if (batch_min < min_) {
        min_ = batch_min;
    }
}

void MaxAccumulator::Update(const Column* column) {
    CellTypes batch_max = column->GetMax();
    if (!has_data_) {
        max_ = batch_max;
        has_data_ = true;
    } else if (batch_max > max_) {
        max_ = batch_max;
    }
}

void MaxAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    CellTypes batch_max = column->GetMax(mask);
    if (!has_data_) {
        max_ = batch_max;
        has_data_ = true;
    } else if (batch_max > max_) {
        max_ = batch_max;
    }
}

void CountDistinctIntAccumulator::Update(const Column* column) {
    const auto* col = static_cast<const Int64*>(column);
    col->FillHashSet(set_);
}

void CountDistinctIntAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    const auto* col = static_cast<const Int64*>(column);
    col->FillHashSet(set_, mask);
}

void CountDistinctStringAccumulator::Update(const Column* column) {
    const auto* col = static_cast<const String*>(column);
    col->FillHashSet(set_);
}

void CountDistinctStringAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    const auto* col = static_cast<const String*>(column);
    col->FillHashSet(set_, mask);
}

void GlobalAggregationOperator::Init() {
    int i = -1;
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    for (auto op : op_) {
        ++i;
        int64_t col_type = scheme_.GetTypeInfo(columns_[i]);
        switch (op) {
            case Op::SUM:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    accumulators_.push_back(std::make_unique<SumIntAccumulator>());
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else {
                    accumulators_.push_back(std::make_unique<SumFloatAccumulator>());
                    result_batch_.value().push_back(std::make_unique<Double>());
                }
                break;
            case Op::MIN:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (col_type == static_cast<int64_t>(Types::TypeDouble)) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                } else {
                    result_batch_.value().push_back(std::make_unique<String>());
                }
                accumulators_.push_back(std::make_unique<MinAccumulator>());
                break;
            case Op::MAX:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (col_type == static_cast<int64_t>(Types::TypeDouble)) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                } else {
                    result_batch_.value().push_back(std::make_unique<String>());
                }
                accumulators_.push_back(std::make_unique<MaxAccumulator>());
                break;
            case Op::AVG:
                accumulators_.push_back(std::make_unique<AvgAccumulator>(std::make_unique<SumIntAccumulator>()));
                result_batch_.value().push_back(std::make_unique<Double>());
                break;
            case Op::COUNT:
                accumulators_.push_back(std::make_unique<CountAccumulator>());
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
            case Op::CountDistinct:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    accumulators_.push_back(std::make_unique<CountDistinctIntAccumulator>());
                } else {
                    accumulators_.push_back(std::make_unique<CountDistinctStringAccumulator>());
                }
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
        }
    }
}

std::optional<Batch> GlobalAggregationOperator::Next() {
    std::vector<int> curr_ids;
    for (auto& el : columns_) {
        curr_ids.push_back(scheme_.GetColumnIndex(el));
    }
    while (const auto& batch = child_->Next()) {
        int i = 0;
        for (int id : curr_ids) {
            accumulators_[i]->Update(batch.value()[id].get());
            ++i;
        }
    }
    for (int64_t i = 0; i < accumulators_.size(); ++i) {
        result_batch_.value()[i]->AddCell(accumulators_[i]->GetResult());
    }
    return std::move(result_batch_);
}

std::vector<std::unique_ptr<IAccumulator>> GroupByAggregationOperator::CreateGroupAccumulators() const {
    std::vector<std::unique_ptr<IAccumulator>> result;
    int i = -1;
    for (auto op : op_) {
        ++i;
        int64_t col_type = scheme_.GetTypeInfo(aggr_col_names_[i]);
        switch (op) {
            case Op::SUM:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result.push_back(std::make_unique<SumIntAccumulator>());
                } else {
                    result.push_back(std::make_unique<SumFloatAccumulator>());
                }
                break;
            case Op::MIN:
                result.push_back(std::make_unique<MinAccumulator>());
                break;
            case Op::MAX:
                result.push_back(std::make_unique<MaxAccumulator>());
                break;
            case Op::AVG:
                result.push_back(std::make_unique<AvgAccumulator>(std::make_unique<SumIntAccumulator>()));
                break;
            case Op::COUNT:
                result.push_back(std::make_unique<CountAccumulator>());
                break;
            case Op::CountDistinct:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result.push_back(std::make_unique<CountDistinctIntAccumulator>());
                } else {
                    result.push_back(std::make_unique<CountDistinctStringAccumulator>());
                }
                break;
        }
    }
    return std::move(result);
}

void GroupByAggregationOperator::InitResultBatch() {
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    for (int64_t i = 0; i < group_by_fields_.size(); ++i) {
        result_batch_.value().push_back(std::make_unique<String>());
    }
    int i = -1;
    for (auto op : op_) {
        ++i;
        int64_t col_type = scheme_.GetTypeInfo(aggr_col_names_[i]);
        switch (op) {
            case Op::SUM:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else {
                    result_batch_.value().push_back(std::make_unique<Double>());
                }
                break;
            case Op::MIN:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (col_type == static_cast<int64_t>(Types::TypeDouble)) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                } else {
                    result_batch_.value().push_back(std::make_unique<String>());
                }
                break;
            case Op::MAX:
                if (col_type == static_cast<int64_t>(Types::TypeInt64)) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (col_type == static_cast<int64_t>(Types::TypeDouble)) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                } else {
                    result_batch_.value().push_back(std::make_unique<String>());
                }
                break;
            case Op::AVG:
                result_batch_.value().push_back(std::make_unique<Double>());
                break;
            case Op::COUNT:
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
            case Op::CountDistinct:
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
        }
    }
}

std::optional<Batch> GroupByAggregationOperator::Next() {
    std::vector<int> aggr_ids;
    std::vector<int> group_by_ids;
    std::unordered_map<uint64_t, int64_t> hash_to_group_id;
    int64_t group_id = -1;
    std::vector<std::vector<std::string>> group_name;
    std::vector<std::vector<std::unique_ptr<IAccumulator>>> group_to_accumulators;
    for (auto& el : aggr_col_names_) {
        aggr_ids.push_back(scheme_.GetColumnIndex(el));
    }
    for (auto& el : group_by_fields_) {
        group_by_ids.push_back(scheme_.GetColumnIndex(el));
    }

    while (auto batch = child_->Next()) {
        std::vector<uint64_t> hashes;
        std::vector<std::vector<std::string>> row_id_to_name;
        std::vector<uint64_t> row_to_group_id;
        int64_t row_count = batch.value()[group_by_ids.front()]->GetRowCount();
        hashes.resize(row_count);
        row_id_to_name.resize(row_count);
        for (int64_t i = 0; i < hashes.size(); ++i) {
            hashes[i] = seed_;
        }
        for (auto id : group_by_ids) {
            batch.value()[id]->MergeHashes(hashes, row_id_to_name);
        }
        uint64_t row_id = 0;
        for (uint64_t hash : hashes) {
            auto it = hash_to_group_id.find(hash);
            if (it == hash_to_group_id.end()) {
                ++group_id;
                hash_to_group_id.emplace(hash, group_id);
                row_to_group_id.push_back(group_id);
                group_name.push_back(std::move(row_id_to_name[row_id]));
                std::vector<std::unique_ptr<IAccumulator>> accumulators = CreateGroupAccumulators();
                group_to_accumulators.push_back(std::move(accumulators));
            } else {
                row_to_group_id.push_back(it->second);
            }
            ++row_id;
        }
        std::unordered_map<uint64_t, std::vector<uint64_t>> groups;
        for (uint64_t row_id = 0; row_id < row_to_group_id.size(); ++row_id) {
            groups[row_to_group_id[row_id]].push_back(row_id);
        }
        for (auto& [id, rows] : groups) {
            int aggr_col_id = 0;
            for (auto& accumulator : group_to_accumulators[id]) {
                accumulator->Update(batch.value()[aggr_ids[aggr_col_id]].get(), rows);
                ++aggr_col_id;
            }
        }
    }
    for (uint64_t i = 0; i <= group_id; ++i) {
        std::vector<std::string> keys = group_name[i];
        for (int64_t j = 0; j < keys.size(); ++j) {
            result_batch_.value()[j]->AddCell(keys[j]);
        }
        int64_t offset = keys.size();
        for (int64_t k = 0; k < aggr_col_names_.size(); ++k) {
            result_batch_.value()[offset + k]->AddCell(group_to_accumulators[i][k]->GetResult());
        }
    }
    return std::move(result_batch_);
}

OrderByLimitKOperator::OrderByLimitKOperator(std::unique_ptr<IOperator> child, int k, bool is_desc, const std::vector<int>& order_by_ids, const Scheme& scheme) : child_(std::move(child)), k_(k), order_by_ids_(order_by_ids), is_desc_(is_desc) {
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    auto all_types = scheme.GetTypesInfo();
    auto curr_ids = child_->GetCurrColIds(); 
    for (auto c : curr_ids) {
        auto el = all_types[c];
        switch (el) {
            case static_cast<int64_t>(Types::TypeInt64):
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
            case static_cast<int64_t>(Types::TypeString):
                result_batch_.value().push_back(std::make_unique<String>());
                break;
            case static_cast<int64_t>(Types::TypeDouble):
                result_batch_.value().push_back(std::make_unique<Double>());
                break;
        }
    }
}

std::optional<Batch> OrderByLimitKOperator::Next() {
    auto curr_ids = child_->GetCurrColIds();
    auto comp = [&](const std::vector<CellTypes>& a, const std::vector<CellTypes>& b) {
        for (size_t i = 0; i < order_by_ids_.size(); ++i) {
            size_t col_idx = order_by_ids_[i];
            
            if (a[col_idx] == b[col_idx]) {
                continue;
            }
            
            bool is_less = a[col_idx] < b[col_idx];
            return is_desc_ ? !is_less : is_less;
        }
        return false;
    };
    std::priority_queue<std::vector<CellTypes>, std::vector<std::vector<CellTypes>>, decltype(comp)> top_k(comp);
    while (auto batch = child_->Next()) {
        int64_t num_rows = batch.value()[0]->GetRowCount();
        int64_t num_cols = batch.value().size();
        for (int64_t r = 0; r < num_rows; ++r) {
            std::vector<CellTypes> current_row;
            current_row.reserve(num_cols);
            for (int64_t c : curr_ids) {
                current_row.push_back(batch.value()[c]->Get(r));
            }
            if (top_k.size() < k_) {
                top_k.push(std::move(current_row));
            } else if (comp(current_row, top_k.top())) {
                top_k.pop();
                top_k.push(std::move(current_row));
            }
        }
    }
    if (top_k.empty()) {
        return std::move(result_batch_);
    }
    std::vector<std::vector<CellTypes>> final_rows;
    final_rows.reserve(top_k.size());
    while (!top_k.empty()) {
        final_rows.push_back(top_k.top());
        top_k.pop();
    }
    std::reverse(final_rows.begin(), final_rows.end());
    for (auto& row : final_rows) {
        for (size_t c = 0; c < curr_ids.size(); ++c) {
            result_batch_.value()[c]->AddCell(std::move(row[c]));
        }
    }
    return std::move(result_batch_);
}

OrderByOperator::OrderByOperator(std::unique_ptr<IOperator> child, const std::vector<int>& order_by_ids, bool is_desc, const Scheme& scheme) : child_(std::move(child)), order_by_ids_(order_by_ids), is_desc_(is_desc) {
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    auto all_types = scheme.GetTypesInfo();
    auto curr_ids = child_->GetCurrColIds();
    for (auto c : curr_ids) {
        auto el = all_types[c];
        switch (el) {
            case static_cast<int64_t>(Types::TypeInt64):
                result_batch_.value().push_back(std::make_unique<Int64>());
                break;
            case static_cast<int64_t>(Types::TypeString):
                result_batch_.value().push_back(std::make_unique<String>());
                break;
            case static_cast<int64_t>(Types::TypeDouble):
                result_batch_.value().push_back(std::make_unique<Double>());
                break;
        }
    }
}

std::optional<Batch> OrderByOperator::Next() {
    std::vector<std::vector<CellTypes>> final_rows;
    auto curr_ids = child_->GetCurrColIds();
    while (auto batch = child_->Next()) {
        int64_t num_rows = batch.value()[0]->GetRowCount();
        int64_t num_cols = batch.value().size();
        for (int64_t r = 0; r < num_rows; ++r) {
            std::vector<CellTypes> current_row;
            current_row.reserve(num_cols);
            for (int64_t c : curr_ids) {
                current_row.push_back(batch.value()[c]->Get(r));
            }
            final_rows.push_back(std::move(current_row));
        }
    }
    auto comp = [&](const std::vector<CellTypes>& a, const std::vector<CellTypes>& b) {
        for (size_t i = 0; i < order_by_ids_.size(); ++i) {
            size_t col_idx = order_by_ids_[i];
            
            if (a[col_idx] == b[col_idx]) {
                continue;
            }
            
            bool is_less = a[col_idx] < b[col_idx];
            return is_desc_ ? !is_less : is_less;
        }
        return false;
    };
    std::sort(final_rows.begin(), final_rows.end(), comp);
    for (auto& row : final_rows) {
        for (size_t c = 0; c < curr_ids.size(); ++c) {
            result_batch_.value()[c]->AddCell(std::move(row[c]));
        }
    }
    return std::move(result_batch_);
}