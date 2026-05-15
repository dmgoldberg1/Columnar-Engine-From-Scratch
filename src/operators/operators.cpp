#include "operators.h"

#include "../utilities/utilities.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <stdexcept>
#include <unordered_map>

namespace {

void EnsureTransformsSize(size_t expected_size, std::vector<AggregationTransform>& transforms) {
    if (transforms.empty()) {
        transforms.resize(expected_size);
        return;
    }
    if (transforms.size() != expected_size) {
        throw std::runtime_error("Aggregation transform count must match aggregation count.");
    }
}

bool IsIntegralType(int64_t type) {
    return type == static_cast<int64_t>(Types::TypeInt16) ||
           type == static_cast<int64_t>(Types::TypeInt32) ||
           type == static_cast<int64_t>(Types::TypeInt64) ||
           type == static_cast<int64_t>(Types::TypeTimestamp);
}

int64_t GetEffectiveType(int64_t source_type, const AggregationTransform& transform) {
    if (!transform.HasValue()) {
        return source_type;
    }
    if (!transform.output_type.has_value()) {
        throw std::runtime_error("Aggregation transform requires output_type.");
    }
    return transform.output_type.value();
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

std::vector<std::string> BuildGroupKeyNames(
    const Batch& batch,
    const std::vector<int>& group_by_ids,
    const std::vector<AggregationTransform>& group_by_transforms,
    int64_t row_id
) {
    std::vector<std::string> result;
    result.reserve(group_by_ids.size());
    for (size_t i = 0; i < group_by_ids.size(); ++i) {
        CellTypes value = batch[group_by_ids[i]]->Get(row_id);
        if (group_by_transforms[i].HasValue()) {
            value = group_by_transforms[i].Apply(value);
        }
        result.push_back(CellToString(value));
    }
    return result;
}

std::unique_ptr<IAccumulator> CreateAccumulator(GlobalAggregationOperator::Op op, int64_t effective_type, const AggregationTransform& transform) {
    switch (op) {
        case GlobalAggregationOperator::Op::SUM:
            if (IsIntegralType(effective_type)) {
                return std::make_unique<SumIntAccumulator>(transform);
            }
            return std::make_unique<SumFloatAccumulator>(transform);
        case GlobalAggregationOperator::Op::AVG:
            if (IsIntegralType(effective_type)) {
                return std::make_unique<AvgAccumulator>(std::make_unique<SumIntAccumulator>(transform));
            }
            return std::make_unique<AvgAccumulator>(std::make_unique<SumFloatAccumulator>(transform));
        case GlobalAggregationOperator::Op::MIN:
            return std::make_unique<MinAccumulator>();
        case GlobalAggregationOperator::Op::MAX:
            return std::make_unique<MaxAccumulator>();
        case GlobalAggregationOperator::Op::COUNT:
            return std::make_unique<CountAccumulator>();
        case GlobalAggregationOperator::Op::CountDistinct:
            if (IsIntegralType(effective_type)) {
                return std::make_unique<CountDistinctIntAccumulator>();
            }
            return std::make_unique<CountDistinctStringAccumulator>();
    }
    throw std::runtime_error("Unknown aggregation op.");
}

void AddResultColumn(Batch& batch, std::vector<int64_t>& curr_types, GlobalAggregationOperator::Op op, int64_t effective_type) {
    switch (op) {
        case GlobalAggregationOperator::Op::SUM:
            if (effective_type == static_cast<int64_t>(Types::TypeInt16)) {
                batch.push_back(std::make_unique<Int16>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt16));
            } else if (effective_type == static_cast<int64_t>(Types::TypeInt32)) {
                batch.push_back(std::make_unique<Int32>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt32));
            } else if (effective_type == static_cast<int64_t>(Types::TypeInt64)) {
                batch.push_back(std::make_unique<Int64>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt64));
            } else if (effective_type == static_cast<int64_t>(Types::TypeTimestamp)) {
                batch.push_back(std::make_unique<Timestamp>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeTimestamp));
            } else {
                batch.push_back(std::make_unique<Double>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeDouble));
            }
            return;
        case GlobalAggregationOperator::Op::AVG:
            batch.push_back(std::make_unique<Double>());
            curr_types.push_back(static_cast<int64_t>(Types::TypeDouble));
            return;
        case GlobalAggregationOperator::Op::MIN:
        case GlobalAggregationOperator::Op::MAX:
            if (effective_type == static_cast<int64_t>(Types::TypeInt16)) {
                batch.push_back(std::make_unique<Int16>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt16));
            } else if (effective_type == static_cast<int64_t>(Types::TypeInt32)) {
                batch.push_back(std::make_unique<Int32>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt32));
            } else if (effective_type == static_cast<int64_t>(Types::TypeInt64)) {
                batch.push_back(std::make_unique<Int64>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeInt64));
            } else if (effective_type == static_cast<int64_t>(Types::TypeTimestamp)) {
                batch.push_back(std::make_unique<Timestamp>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeTimestamp));
            } else if (effective_type == static_cast<int64_t>(Types::TypeDouble)) {
                batch.push_back(std::make_unique<Double>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeDouble));
            } else if (effective_type == static_cast<int64_t>(Types::TypeDate)) {
                batch.push_back(std::make_unique<Date>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeDate));
            } else {
                batch.push_back(std::make_unique<String>());
                curr_types.push_back(static_cast<int64_t>(Types::TypeString));
            }
            return;
        case GlobalAggregationOperator::Op::COUNT:
        case GlobalAggregationOperator::Op::CountDistinct:
            batch.push_back(std::make_unique<Int64>());
            curr_types.push_back(static_cast<int64_t>(Types::TypeInt64));
            return;
    }
}

} // namespace

ScanOperator::ScanOperator(const std::string& filename, const std::vector<std::string>& columns) 
    : columns_(columns),
      file_(filename, std::ios::binary | std::ios::ate),
      reader_(file_) {
        auto all_types = reader_.GetScheme().GetTypesInfo();
        for (const auto& name : columns_) {
            int id = reader_.GetScheme().GetColumnIndex(name);
            curr_ids_.push_back(id);
            curr_types_.push_back(all_types[id]);
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
    std::vector<int> curr_ids = child_->GetCurrColIds();
    std::optional<Batch> batch = child_->Next();
    if (!batch.has_value()) {
        return std::nullopt;
    }
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
    if (const auto* int_column = dynamic_cast<const Int64*>(column)) {
        if (!transform_.HasValue()) {
            sum_ += static_cast<__int128_t>(int_column->GetSum());
            return;
        }
        auto transform = [this](int64_t value) -> int64_t {
            return std::get<int64_t>(transform_.Apply(value));
        };
        sum_ += static_cast<__int128_t>(int_column->GetSum(transform));
        return;
    }
    int64_t row_count = column->GetRowCount();
    for (int64_t i = 0; i < row_count; ++i) {
        CellTypes value = transform_.HasValue() ? transform_.Apply(column->Get(i)) : column->Get(i);
        sum_ += static_cast<__int128_t>(std::get<int64_t>(value));
    }
}

void SumIntAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    if (const auto* int_column = dynamic_cast<const Int64*>(column)) {
        if (!transform_.HasValue()) {
            sum_ += static_cast<__int128_t>(int_column->GetSum(mask));
            return;
        }
        auto transform = [this](int64_t value) -> int64_t {
            return std::get<int64_t>(transform_.Apply(value));
        };
        sum_ += static_cast<__int128_t>(int_column->GetSum(mask, transform));
        return;
    }
    for (uint64_t row_id : mask) {
        CellTypes value = transform_.HasValue() ? transform_.Apply(column->Get(row_id)) : column->Get(row_id);
        sum_ += static_cast<__int128_t>(std::get<int64_t>(value));
    }
}



void SumFloatAccumulator::Update(const Column* column) {
    if (const auto* double_column = dynamic_cast<const Double*>(column)) {
        if (!transform_.HasValue()) {
            sum_ += double_column->GetSum();
            return;
        }
        auto transform = [this](double value) -> double {
            return std::get<double>(transform_.Apply(value));
        };
        sum_ += double_column->GetSum(transform);
        return;
    }
    int64_t row_count = column->GetRowCount();
    for (int64_t i = 0; i < row_count; ++i) {
        CellTypes value = transform_.HasValue() ? transform_.Apply(column->Get(i)) : column->Get(i);
        sum_ += std::visit([](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
                return static_cast<double>(arg);
            } else {
                throw std::runtime_error("SumFloatAccumulator expects numeric input.");
            }
        }, value);
    }
}

void SumFloatAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    if (const auto* double_column = dynamic_cast<const Double*>(column)) {
        if (!transform_.HasValue()) {
            sum_ += double_column->GetSum(mask);
            return;
        }
        auto transform = [this](double value) -> double {
            return std::get<double>(transform_.Apply(value));
        };
        sum_ += double_column->GetSum(mask, transform);
        return;
    }
    for (uint64_t row_id : mask) {
        CellTypes value = transform_.HasValue() ? transform_.Apply(column->Get(row_id)) : column->Get(row_id);
        sum_ += std::visit([](auto&& arg) -> double {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, int64_t> || std::is_same_v<T, double>) {
                return static_cast<double>(arg);
            } else {
                throw std::runtime_error("SumFloatAccumulator expects numeric input.");
            }
        }, value);
    }
}

CellTypes AvgAccumulator::GetResult() const {
    if (count_ == 0) {
        return 0;
    }
    if (const auto* int_sum_accumulator = dynamic_cast<const SumIntAccumulator*>(sum_accumulator_.get())) {
        long double average = static_cast<long double>(int_sum_accumulator->GetWideResult()) / static_cast<long double>(count_);
        return static_cast<double>(average);
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
    if (const auto* col = dynamic_cast<const Int16*>(column)) {
        col->FillHashSet(set_);
        return;
    }
    if (const auto* col = dynamic_cast<const Int32*>(column)) {
        col->FillHashSet(set_);
        return;
    }
    if (const auto* col = dynamic_cast<const Int64*>(column)) {
        col->FillHashSet(set_);
        return;
    }
    if (const auto* col = dynamic_cast<const Timestamp*>(column)) {
        col->FillHashSet(set_);
        return;
    }
    throw std::runtime_error("CountDistinctIntAccumulator expects an integral column.");
}

void CountDistinctIntAccumulator::Update(const Column* column, const std::vector<uint64_t>& mask) {
    if (const auto* col = dynamic_cast<const Int16*>(column)) {
        col->FillHashSet(set_, mask);
        return;
    }
    if (const auto* col = dynamic_cast<const Int32*>(column)) {
        col->FillHashSet(set_, mask);
        return;
    }
    if (const auto* col = dynamic_cast<const Int64*>(column)) {
        col->FillHashSet(set_, mask);
        return;
    }
    if (const auto* col = dynamic_cast<const Timestamp*>(column)) {
        col->FillHashSet(set_, mask);
        return;
    }
    throw std::runtime_error("CountDistinctIntAccumulator expects an integral column.");
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
    EnsureTransformsSize(columns_.size(), transforms_);
    int i = -1;
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    curr_types_.clear();
    for (auto op : op_) {
        ++i;
        int64_t source_type = scheme_.GetTypeInfo(columns_[i]);
        int64_t effective_type = GetEffectiveType(source_type, transforms_[i]);
        accumulators_.push_back(CreateAccumulator(op, effective_type, transforms_[i]));
        AddResultColumn(result_batch_.value(), curr_types_, op, effective_type);
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
        int64_t source_type = scheme_.GetTypeInfo(aggr_col_names_[i]);
        int64_t effective_type = GetEffectiveType(source_type, transforms_[i]);
        result.push_back(CreateAccumulator(op, effective_type, transforms_[i]));
    }
    return std::move(result);
}

void GroupByAggregationOperator::InitResultBatch() {
    EnsureTransformsSize(aggr_col_names_.size(), transforms_);
    EnsureTransformsSize(group_by_fields_.size(), group_by_transforms_);
    result_batch_ = std::vector<std::unique_ptr<Column>>();
    curr_types_.clear();
    for (int64_t i = 0; i < group_by_fields_.size(); ++i) {
        result_batch_.value().push_back(std::make_unique<String>());
        curr_types_.push_back(static_cast<int64_t>(Types::TypeString));
    }
    int i = -1;
    for (auto op : op_) {
        ++i;
        int64_t source_type = scheme_.GetTypeInfo(aggr_col_names_[i]);
        int64_t effective_type = GetEffectiveType(source_type, transforms_[i]);
        AddResultColumn(result_batch_.value(), curr_types_, op, effective_type);
    }
}

std::optional<Batch> GroupByAggregationOperator::Next() {
    if (is_consumed_) {
        return std::nullopt;
    }
    EnsureTransformsSize(group_by_fields_.size(), group_by_transforms_);
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
        std::vector<std::vector<std::string>> empty_group_names;
        int64_t row_count = batch.value()[group_by_ids.front()]->GetRowCount();
        if (row_count == 0) {
            continue;
        }
        hashes.resize(row_count);
        for (int64_t i = 0; i < hashes.size(); ++i) {
            hashes[i] = seed_;
        }
        for (size_t i = 0; i < group_by_ids.size(); ++i) {
            const AggregationTransform& transform = group_by_transforms_[i];
            batch.value()[group_by_ids[i]]->MergeHashes(
                hashes,
                empty_group_names,
                transform.HasValue() ? transform.fn : std::function<CellTypes(const CellTypes&)>{}
            );
        }
        std::vector<uint64_t> single_row(1);
        for (uint64_t row_id = 0; row_id < hashes.size(); ++row_id) {
            uint64_t hash = hashes[row_id];
            auto it = hash_to_group_id.find(hash);
            uint64_t current_group_id;
            if (it == hash_to_group_id.end()) {
                ++group_id;
                hash_to_group_id.emplace(hash, group_id);
                current_group_id = group_id;
                group_name.push_back(BuildGroupKeyNames(batch.value(), group_by_ids, group_by_transforms_, row_id));
                std::vector<std::unique_ptr<IAccumulator>> accumulators = CreateGroupAccumulators();
                group_to_accumulators.push_back(std::move(accumulators));
            } else {
                current_group_id = it->second;
            }
            single_row[0] = row_id;
            int aggr_col_id = 0;
            for (auto& accumulator : group_to_accumulators[current_group_id]) {
                accumulator->Update(batch.value()[aggr_ids[aggr_col_id]].get(), single_row);
                ++aggr_col_id;
            }
        }
    }
    if (group_id < 0) {
        is_consumed_ = true;
        return std::move(result_batch_);
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
    is_consumed_ = true;
    return std::move(result_batch_);
}

OrderByLimitKOperator::OrderByLimitKOperator(std::unique_ptr<IOperator> child, int k, bool is_desc, const std::vector<int>& order_by_ids, const Scheme& scheme) : child_(std::move(child)), k_(k), order_by_ids_(order_by_ids), is_desc_(is_desc) {
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
        if (!result_batch_.has_value()) {
            result_batch_ = std::vector<std::unique_ptr<Column>>();
            for (int64_t c : curr_ids) {
                if (dynamic_cast<const Int16*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int16>());
                } else if (dynamic_cast<const Int32*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int32>());
                } else if (dynamic_cast<const Int64*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (dynamic_cast<const Date*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Date>());
                } else if (dynamic_cast<const Timestamp*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Timestamp>());
                } else if (dynamic_cast<const String*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<String>());
                } else if (dynamic_cast<const Double*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                }
            }
        }
        int64_t num_rows = batch.value()[curr_ids.front()]->GetRowCount();
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
    if (!result_batch_.has_value()) {
        return std::nullopt;
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
}

std::optional<Batch> OrderByOperator::Next() {
    std::vector<std::vector<CellTypes>> final_rows;
    auto curr_ids = child_->GetCurrColIds();
    while (auto batch = child_->Next()) {
        if (!result_batch_.has_value()) {
            result_batch_ = std::vector<std::unique_ptr<Column>>();
            for (int64_t c : curr_ids) {
                if (dynamic_cast<const Int16*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int16>());
                } else if (dynamic_cast<const Int32*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int32>());
                } else if (dynamic_cast<const Int64*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Int64>());
                } else if (dynamic_cast<const Date*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Date>());
                } else if (dynamic_cast<const Timestamp*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Timestamp>());
                } else if (dynamic_cast<const String*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<String>());
                } else if (dynamic_cast<const Double*>(batch.value()[c].get()) != nullptr) {
                    result_batch_.value().push_back(std::make_unique<Double>());
                }
            }
        }
        int64_t num_rows = batch.value()[curr_ids.front()]->GetRowCount();
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
    if (!result_batch_.has_value()) {
        return std::nullopt;
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
