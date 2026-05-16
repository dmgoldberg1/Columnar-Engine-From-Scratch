#pragma once

#include "../column_types/column_types.h"
#include "../file_reader/file_reader.h"

#include <functional>
#include <optional>
#include <unordered_set>

using Batch = std::vector<std::unique_ptr<Column>>;

class IOperator {
public:
    virtual std::optional<Batch> Next() = 0;
    virtual std::vector<int> GetCurrColIds() const = 0;
    virtual std::vector<int64_t> GetCurrColTypes() const = 0;
    virtual void SetBatchFilter(const class FilterCondition* condition) {}
    virtual ~IOperator() = default;
};

class ScanOperator : public IOperator {
public:
    ScanOperator(const std::string& filename, const std::vector<std::string>& columns);
    std::vector<int> GetCurrColIds() const override {
        return curr_ids_;
    }
    std::vector<int64_t> GetCurrColTypes() const override {
        return curr_types_;
    }
    void SetBatchFilter(const class FilterCondition* condition) override { batch_filter_ = condition; }

    std::optional<Batch> Next() override;
protected:
    std::vector<std::string> columns_;
    std::ifstream file_;
    RowGroupReader reader_;
    std::vector<int> curr_ids_;
    std::vector<int64_t> curr_types_;
    const class FilterCondition* batch_filter_ = nullptr;
};

class FilterCondition {
public:
    virtual ~FilterCondition() = default;
    virtual bool Evaluate(const Batch& batch, size_t row_index) const = 0;
    virtual bool CanSkipBatch(const BatchBlockStats& batch_stats) const { return false; }
};

template<typename T>
class CompareFilter : public FilterCondition {
public:
    using Op = Column::Op;
    CompareFilter(const std::string& column, Op op, T value, Scheme scheme) : column_(column), op_(op), value_(value), scheme_(scheme) {}
    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return batch[scheme_.GetColumnIndex(column_)]->Compare(row_index, op_, value_);
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        const int column_index = scheme_.GetColumnIndex(column_);
        if (column_index < 0 || column_index >= static_cast<int>(batch_stats.size())) {
            return false;
        }
        const ColumnBlockStats& stats = batch_stats[column_index];
        const CellTypes value = CellTypes(value_);

        switch (op_) {
            case Op::EQ:
                return stats.min_value > value || stats.max_value < value;
            case Op::NE:
                return stats.min_value == value && stats.max_value == value;
            case Op::LT:
                return stats.min_value >= value;
            case Op::LE:
                return stats.min_value > value;
            case Op::GT:
                return stats.max_value <= value;
            case Op::GE:
                return stats.max_value < value;
        }
        return false;
    }
protected:
    std::string column_;
    Op op_;
    T value_;
    Scheme scheme_;
};

class CompareFilterByIndex : public FilterCondition {
public:
    using Op = Column::Op;

    CompareFilterByIndex(int column_index, Op op, CellTypes value)
        : column_index_(column_index), op_(op), value_(std::move(value)) {}

    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return batch[column_index_]->Compare(row_index, op_, value_);
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        return false;
    }

protected:
    int column_index_;
    Op op_;
    CellTypes value_;
};

class LikeFilter : public FilterCondition {
public:
    LikeFilter(const std::string& column, std::string pattern, Scheme scheme)
        : column_(column), pattern_(std::move(pattern)), scheme_(scheme) {}

    bool Evaluate(const Batch& batch, size_t row_index) const override {
        const std::string value = batch[scheme_.GetColumnIndex(column_)]->GetCellAsString(row_index);
        return value.find(pattern_) != std::string::npos;
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        return false;
    }

protected:
    std::string column_;
    std::string pattern_;
    Scheme scheme_;
};

class NotFilter : public FilterCondition {
public:
    explicit NotFilter(std::unique_ptr<FilterCondition> child) : child_(std::move(child)) {}

    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return !child_->Evaluate(batch, row_index);
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        return false;
    }

protected:
    std::unique_ptr<FilterCondition> child_;
};

class AndFilter : public FilterCondition {
public:
    AndFilter(std::unique_ptr<FilterCondition> left, std::unique_ptr<FilterCondition> right) : left_(std::move(left)), right_(std::move(right)) {}
    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return left_->Evaluate(batch, row_index) && right_->Evaluate(batch, row_index);
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        return left_->CanSkipBatch(batch_stats) || right_->CanSkipBatch(batch_stats);
    }

protected:
    std::unique_ptr<FilterCondition> left_;
    std::unique_ptr<FilterCondition> right_;

};

class OrFilter : public FilterCondition {
public:
    OrFilter(std::unique_ptr<FilterCondition> left, std::unique_ptr<FilterCondition> right) : left_(std::move(left)), right_(std::move(right)) {}
    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return left_->Evaluate(batch, row_index) || right_->Evaluate(batch, row_index);
    }
    bool CanSkipBatch(const BatchBlockStats& batch_stats) const override {
        return left_->CanSkipBatch(batch_stats) && right_->CanSkipBatch(batch_stats);
    }

protected:
    std::unique_ptr<FilterCondition> left_;
    std::unique_ptr<FilterCondition> right_;

};


class FilterOperator : public IOperator {
public:
    FilterOperator(std::unique_ptr<IOperator> child, std::unique_ptr<FilterCondition> cond) : child_(std::move(child)), condition_(std::move(cond)) {
        child_->SetBatchFilter(condition_.get());
    }
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override;
    std::vector<int64_t> GetCurrColTypes() const override { return child_->GetCurrColTypes(); }
    void SetBatchFilter(const FilterCondition* condition) override { child_->SetBatchFilter(condition); }
protected:
    std::unique_ptr<IOperator> child_;
    std::unique_ptr<FilterCondition> condition_;
};

struct AggregationTransform {
    using Fn = std::function<CellTypes(const CellTypes&)>;

    Fn fn;
    std::optional<int64_t> output_type;

    bool HasValue() const {
        return static_cast<bool>(fn);
    }

    CellTypes Apply(const CellTypes& value) const {
        if (!HasValue()) {
            return value;
        }
        return fn(value);
    }
};

class IAccumulator {
public:
    virtual ~IAccumulator() = default;
    virtual void Update(const Column* column) = 0;
    virtual void Update(const Column* column, const std::vector<uint64_t>& mask) = 0;
    virtual CellTypes GetResult() const = 0;
};

class SumIntAccumulator : public IAccumulator {
public:
    explicit SumIntAccumulator(AggregationTransform transform = {}) : transform_(std::move(transform)) {}
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override { return static_cast<int64_t>(sum_); }
    __int128_t GetWideResult() const { return sum_; }
protected:
    AggregationTransform transform_;
    __int128_t sum_ = 0;
};

class SumFloatAccumulator : public IAccumulator {
public:
    explicit SumFloatAccumulator(AggregationTransform transform = {}) : transform_(std::move(transform)) {}
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const { return sum_; }
protected:
    AggregationTransform transform_;
    double sum_ = 0;
};

class AvgAccumulator : public IAccumulator {
public:
    AvgAccumulator(std::unique_ptr<IAccumulator> sum_accumulator) : sum_accumulator_(std::move(sum_accumulator)) {}
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override;
protected:
    std::unique_ptr<IAccumulator> sum_accumulator_;
    uint32_t count_ = 0;
};

class CountAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override {
        return count_;
    }
protected:
    int64_t count_ = 0;
};

class MinAccumulator : public IAccumulator {
public:
    MinAccumulator() = default;
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override {
        return min_;
    }

protected:
    CellTypes min_;
    bool has_data_ = false;
};

class MaxAccumulator : public IAccumulator {
public:
    MaxAccumulator() = default;
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override {
        return max_;
    }

protected:
    CellTypes max_;
    bool has_data_ = false;
};



class CountDistinctIntAccumulator : public IAccumulator {
public:
    CountDistinctIntAccumulator() = default;
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override {
        return static_cast<int64_t>(set_.size());
    }
protected:
    std::unordered_set<int64_t> set_;
};

class CountDistinctStringAccumulator : public IAccumulator {
public:
    CountDistinctStringAccumulator() = default;
    void Update(const Column* column) override;
    void Update(const Column* column, const std::vector<uint64_t>& mask) override;
    CellTypes GetResult() const override {
        return static_cast<int64_t>(set_.size());
    }
protected:
    std::unordered_set<std::string> set_;
};


class GlobalAggregationOperator : public IOperator {
public:
    enum class Op {SUM, AVG, COUNT, MIN, MAX, CountDistinct};
    GlobalAggregationOperator(const std::vector<std::string>& columns, std::unique_ptr<IOperator> child, std::vector<Op> op, const Scheme& scheme, std::vector<AggregationTransform> transforms = {})
        : columns_(columns), child_(std::move(child)), op_(op), scheme_(scheme), transforms_(std::move(transforms)) {
        Init();
    }
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override {
        std::vector<int> result;
        for (int i = 0; i < columns_.size(); ++i) {
            result.push_back(i);
        }
        return result;
    }
    std::vector<int64_t> GetCurrColTypes() const override { return curr_types_; }

protected:
    void Init();
protected:
    std::vector<std::unique_ptr<IAccumulator>> accumulators_;
    std::vector<std::string> columns_;
    std::unique_ptr<IOperator> child_;
    std::vector<Op> op_;
    Scheme scheme_;
    std::optional<Batch> result_batch_;
    std::vector<int64_t> curr_types_;
    std::vector<AggregationTransform> transforms_;
};

class GroupByAggregationOperator : public IOperator {
public:
    using Op = GlobalAggregationOperator::Op;
    GroupByAggregationOperator(
        std::unique_ptr<IOperator> child,
        const std::vector<std::string>& group_by_fields,
        const std::vector<std::string>& aggr_col_names,
        const std::vector<Op>& op,
        const Scheme& scheme,
        std::vector<AggregationTransform> transforms = {},
        std::vector<AggregationTransform> group_by_transforms = {}
    )
        : child_(std::move(child)),
          group_by_fields_(group_by_fields),
          aggr_col_names_(aggr_col_names),
          op_(op),
          scheme_(scheme),
          transforms_(std::move(transforms)),
          group_by_transforms_(std::move(group_by_transforms)) {
        InitResultBatch();
    }
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override {
        std::vector<int> result;
        for (int i = 0; i < curr_types_.size(); ++i) {
            result.push_back(i);
        }
        return result;
    }
    std::vector<int64_t> GetCurrColTypes() const override { return curr_types_; }
protected:
    std::vector<std::unique_ptr<IAccumulator>> CreateGroupAccumulators() const;
    void InitResultBatch();
protected:
    std::vector<Op> op_;
    std::vector<std::string> group_by_fields_;
    std::vector<std::string> aggr_col_names_;
    std::unique_ptr<IOperator> child_;
    Scheme scheme_;
    int64_t seed_ = 0x9e3779b9;
    std::optional<Batch> result_batch_;
    std::vector<int64_t> curr_types_;
    bool is_consumed_ = false;
    std::vector<AggregationTransform> transforms_;
    std::vector<AggregationTransform> group_by_transforms_;
};

class OrderByLimitKOperator : public IOperator {
public:
    OrderByLimitKOperator(std::unique_ptr<IOperator> child, int k, bool is_desc, const std::vector<int>& order_by_ids, const Scheme& scheme);
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override { return child_->GetCurrColIds(); }
    std::vector<int64_t> GetCurrColTypes() const override { return child_->GetCurrColTypes(); }
protected:
    std::unique_ptr<IOperator> child_;
    int k_;
    std::vector<int> order_by_ids_;
    bool is_desc_;
    std::optional<Batch> result_batch_;
};

class OrderByOperator : public IOperator {
public:
    OrderByOperator(std::unique_ptr<IOperator> child, const std::vector<int>& order_by_ids, bool is_desc, const Scheme& scheme);
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override { return child_->GetCurrColIds(); }
    std::vector<int64_t> GetCurrColTypes() const override { return child_->GetCurrColTypes(); }
protected:
    std::unique_ptr<IOperator> child_;
    std::vector<int> order_by_ids_;
    bool is_desc_;
    std::optional<Batch> result_batch_;
};
