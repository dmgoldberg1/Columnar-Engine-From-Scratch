#include "file_writer.h"
#include "column_types.h"

#include <vector>
#include <string>

static inline constexpr int64_t RowGroupSize = 128 * 1024 * 1024;
RowGroupWriter::RowGroupWriter(CSVWrapper&& reader, std::ostream& output) : impl_(std::make_unique<Impl>(std::move(reader), output)) {
}

class RowGroupWriter::Impl {
public:
    Impl(CSVWrapper&& reader, std::ostream& output) : output_(output) {
        csv_reader_ = std::move(reader);
        csv_reader_.SetColumnNumAndTypes();
        column_num_ = csv_reader_.GetColumnNum();
        std::vector<int64_t> types = csv_reader_.GetTypesInfo();
        for (int64_t i = 0; i < column_num_; ++i) {
            switch (types[i]) {
                case 1:
                    row_group_.push_back(std::make_unique<Int64>());
                    break;
                case 2:
                    row_group_.push_back(std::make_unique<String>());
                    break;
                default:
                    break;
            }
        }
        batch_metadata_.resize(1 + column_num_);
    }

    ~Impl() = default;

    void WriteAll() {
        while (!csv_reader_.IsEnd()) {
            ++batch_count_;
            batch_start_pos_.push_back(output_.tellp());
            WriteGroup();
        }
        output_.write(reinterpret_cast<const char*>(&batch_count_), sizeof(batch_count_));
        WriteMetadata(batch_start_pos_);
        output_.write(reinterpret_cast<const char*>(&column_num_), sizeof(column_num_));
        WriteMetadata(csv_reader_.GetTypesInfo());
        metadata_size_ = (batch_start_pos_.size() + csv_reader_.GetTypesInfo().size() + 2) * sizeof(int64_t);
        output_.write(reinterpret_cast<const char*>(&metadata_size_), sizeof(metadata_size_));
    }

protected:
    void WriteGroup() {
        group_capacity_ = 0;
        int64_t row_count = 0;
        while (group_capacity_ <= RowGroupSize && !csv_reader_.IsEnd()) {
            std::vector<std::unique_ptr<Column>> row = csv_reader_.GetNextLineAndSplitIntoTokens();
            int64_t row_size = GetRowByteSize(row); // maybe calc it in upper func
            group_capacity_ += row_size;
            for (int64_t i = 0; i < column_num_; ++i) {
                row_group_[i]->AddCell(row[i]);
            }
            ++row_count;
        }
        batch_metadata_[0] = group_capacity_;
        for (int64_t i = 1; i < column_num_ + 1; ++i) {
            batch_metadata_[i] = row_group_[i - 1]->GetColumnByteSize();
        }
        batch_metadata_.push_back(row_count);
        WriteMetadata(batch_metadata_);
        for (const auto& column : row_group_) {
            column->Write(output_);
        }
        for (auto& elem : row_group_) {
            elem->Clear();
        }
        
    }

    void WriteMetadata(const std::vector<int64_t>& metadata) {
        for (const int64_t& elem : metadata) {
            output_.write(reinterpret_cast<const char*>(&elem), sizeof(elem));
        }
    }

    int64_t GetRowByteSize(const std::vector<std::unique_ptr<Column>>& row) {
        int64_t size = 0;
        for (const auto& elem : row) {
            size += elem->GetFirstCellSize();
        }
        return size;
    }

protected:
    CSVWrapper csv_reader_;
    std::ostream& output_;
    int64_t group_capacity_ = 0;
    int64_t metadata_size_ = 0;
    int64_t column_num_ = 0;
    int64_t batch_count_ = 0;
    std::vector<std::unique_ptr<Column>> row_group_;
    std::vector<int64_t> batch_start_pos_;
    std::vector<int64_t> batch_metadata_;

};

void RowGroupWriter::WriteAll() {
    impl_->WriteAll();
}

RowGroupWriter::~RowGroupWriter() = default;
