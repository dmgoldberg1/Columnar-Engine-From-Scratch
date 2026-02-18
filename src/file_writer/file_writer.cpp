#include "file_writer.h"
#include "../column_types/column_types.h"
#include "../utilities/utilities.h"

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
        types_ = csv_reader_.GetTypesInfo();
        for (int64_t i = 0; i < column_num_; ++i) {
            switch (types_[i]) {
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
    }

    ~Impl() = default;

    void WriteAll() {
        std::vector<int64_t> file_metadata;
        std::vector<int64_t> batch_start_pos;
        int64_t batch_count = 0;
        while (!csv_reader_.IsEnd()) {
            ++batch_count;
            batch_start_pos.push_back(output_.tellp());
            WriteGroup();
        }
        file_metadata.push_back(batch_count);
        file_metadata.insert(file_metadata.end(), batch_start_pos.begin(), batch_start_pos.end());
        file_metadata.push_back(column_num_);
        file_metadata.insert(file_metadata.end(), types_.begin(), types_.end());
        file_metadata.insert(file_metadata.end(), all_batch_metadata_.begin(), all_batch_metadata_.end());
        file_metadata.push_back(file_metadata.size() * sizeof(int64_t));
        output_.write(reinterpret_cast<const char*>(file_metadata.data()), file_metadata.size() * sizeof(int64_t));
        csv_reader_.Close();
    }

protected:
    void WriteGroup() {
        int64_t group_capacity = 0;
        int64_t row_count = 0;
        while (group_capacity <= RowGroupSize && !csv_reader_.IsEnd()) {
            std::vector<std::string> row = csv_reader_.GetNextLineAndSplitIntoTokens();
            for (int64_t i = 0; i < column_num_; ++i) {
                row_group_[i]->AddCell(row[i]);
                group_capacity += row_group_[i]->GetLastCellSize();
            }
            ++row_count;
        }
        all_batch_metadata_.push_back(group_capacity);
        for (int64_t i = 0; i < column_num_; ++i) {
            all_batch_metadata_.push_back(row_group_[i]->GetColumnByteSize());
        }
        all_batch_metadata_.push_back(row_count);
        for (const auto& column : row_group_) {
            column->Write(output_);
        }
        for (auto& elem : row_group_) {
            elem->Clear();
        }
        
    }

protected:
    CSVWrapper csv_reader_;
    std::ostream& output_;
    int64_t column_num_ = 0;
    std::vector<std::unique_ptr<Column>> row_group_;
    std::vector<int64_t> all_batch_metadata_;
    std::vector<int64_t> types_;

};

void RowGroupWriter::WriteAll() {
    impl_->WriteAll();
}

RowGroupWriter::~RowGroupWriter() = default;
