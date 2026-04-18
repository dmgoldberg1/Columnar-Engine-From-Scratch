#include "operators.h"

#include "../utilities/utilities.h"

#include <iostream>

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

void SumFloatAccumulator::Update(const Column* column) {

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

void CountAccumulator::Update(const Column* column) {
    count_ += column->GetRowCount();
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

void MaxAccumulator::Update(const Column* column) {
    CellTypes batch_max = column->GetMax();
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

void CountDistinctStringAccumulator::Update(const Column* column) {
    const auto* col = static_cast<const String*>(column);
    col->FillHashSet(set_);
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