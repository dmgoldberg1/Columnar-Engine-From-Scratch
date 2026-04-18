#pragma once

#include "../column_types/column_types.h"
#include "../file_reader/file_reader.h"

#include <optional>
#include <unordered_set>

using Batch = std::vector<std::unique_ptr<Column>>;

// TODO: remove fucking GetCurrColIds
class IOperator {
public:
    virtual std::optional<Batch> Next() = 0;
    virtual std::vector<int> GetCurrColIds() const = 0;
    virtual ~IOperator() = default;
};

class ScanOperator : public IOperator {
public:
    ScanOperator(const std::string& filename, const std::vector<std::string>& columns);
    std::vector<int> GetCurrColIds() const override {
        return curr_ids_;
    }
          
    std::optional<Batch> Next() override;
protected:
    std::vector<std::string> columns_;
    std::ifstream file_;
    RowGroupReader reader_;
    std::vector<int> curr_ids_;
};

class FilterCondition {
public:
    virtual ~FilterCondition() = default;
    virtual bool Evaluate(const Batch& batch, size_t row_index) const = 0;
};

template<typename T>
class CompareFilter : public FilterCondition {
public:
    using Op = Column::Op;
    CompareFilter(const std::string& column, Op op, T value, Scheme scheme) : column_(column), op_(op), value_(value), scheme_(scheme) {}
    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return batch[scheme_.GetColumnIndex(column_)]->Compare(row_index, op_, value_);
    }
protected:
    std::string column_;
    Op op_;
    T value_;
    Scheme scheme_;
};

class AndFilter : public FilterCondition {
public:
    AndFilter(std::unique_ptr<FilterCondition> left, std::unique_ptr<FilterCondition> right) : left_(std::move(left)), right_(std::move(right)) {}
    bool Evaluate(const Batch& batch, size_t row_index) const override {
        return left_->Evaluate(batch, row_index) && right_->Evaluate(batch, row_index);
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

protected:
    std::unique_ptr<FilterCondition> left_;
    std::unique_ptr<FilterCondition> right_;

};


class FilterOperator : public IOperator {
public:
    FilterOperator(std::unique_ptr<IOperator> child, std::unique_ptr<FilterCondition> cond) : child_(std::move(child)), condition_(std::move(cond)) {}
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override;
protected:
    std::unique_ptr<IOperator> child_;
    std::unique_ptr<FilterCondition> condition_;
};

class IAccumulator {
public:
    virtual ~IAccumulator() = default;
    virtual void Update(const Column* column) = 0;
    virtual CellTypes GetResult() const = 0;
};

class SumIntAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override { return sum_; }
protected:
    int64_t sum_ = 0;
};

class SumFloatAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const { return sum_; }
protected:
    double sum_ = 0;
};

class AvgAccumulator : public IAccumulator {
public:
    AvgAccumulator(std::unique_ptr<IAccumulator> sum_accumulator) : sum_accumulator_(std::move(sum_accumulator)) {}
    void Update(const Column* column) override;
    CellTypes GetResult() const override;
protected:
    std::unique_ptr<IAccumulator> sum_accumulator_;
    uint32_t count_ = 0;
};

class CountAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override {
        return count_;
    }
protected:
    int64_t count_ = 0;
};

class MinAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override {
        return min_;
    }

protected:
    CellTypes min_;
    bool has_data_ = false;
};

class MaxAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override {
        return max_;
    }

protected:
    CellTypes max_;
    bool has_data_ = false;
};



class CountDistinctIntAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override {
        return static_cast<int64_t>(set_.size());
    }
protected:
    std::unordered_set<int64_t> set_;
};

class CountDistinctStringAccumulator : public IAccumulator {
public:
    void Update(const Column* column) override;
    CellTypes GetResult() const override {
        return static_cast<int64_t>(set_.size());
    }
protected:
    std::unordered_set<std::string> set_;
};


class GlobalAggregationOperator : public IOperator {
public:
    enum class Op {SUM, AVG, COUNT, MIN, MAX, CountDistinct};
    GlobalAggregationOperator(const std::vector<std::string>& columns, std::unique_ptr<IOperator> child, std::vector<Op> op, const Scheme& scheme) : columns_(columns), child_(std::move(child)), op_(op), scheme_(scheme) {
        Init();
    }
    std::optional<Batch> Next() override;
    std::vector<int> GetCurrColIds() const override { return child_->GetCurrColIds(); }

protected:
    void Init();
protected:
    std::vector<std::unique_ptr<IAccumulator>> accumulators_;
    std::vector<std::string> columns_;
    std::unique_ptr<IOperator> child_;
    std::vector<Op> op_;
    Scheme scheme_;
    std::optional<Batch> result_batch_;
};

