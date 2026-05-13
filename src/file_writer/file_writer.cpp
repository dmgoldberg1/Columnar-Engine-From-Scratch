#include "file_writer.h"

#include "../column_types/column_types.h"
#include "../utilities/utilities.h"


#include <vector>
#include <string>
#include <iostream>

static inline constexpr int64_t RowGroupSize = 128 * 1024 * 1024;
RowGroupWriter::RowGroupWriter(CSVWrapper&& reader, std::ostream& output, Scheme& scheme) : impl_(std::make_unique<Impl>(std::move(reader), output, scheme)) {
}

class RowGroupWriter::Impl {
public:
    Impl(CSVWrapper&& reader, std::ostream& output, Scheme& scheme) : output_(output), scheme_(scheme) {
        csv_reader_ = std::move(reader);
        column_num_ = csv_reader_.GetColumnNum();
        types_ = scheme.GetTypesInfo();
        for (int64_t i = 0; i < column_num_; ++i) {
            switch (types_[i]) {
                case static_cast<int64_t>(Types::TypeInt16):
                    row_group_.push_back(std::make_unique<Int16>());
                    break;
                case static_cast<int64_t>(Types::TypeInt32):
                    row_group_.push_back(std::make_unique<Int32>());
                    break;
                case static_cast<int64_t>(Types::TypeInt64):
                    row_group_.push_back(std::make_unique<Int64>());
                    break;
                case static_cast<int64_t>(Types::TypeString):
                    row_group_.push_back(std::make_unique<String>());
                    break;
                case static_cast<int64_t>(Types::TypeDouble):
                    row_group_.push_back(std::make_unique<Double>());
                    break;
                case static_cast<int64_t>(Types::TypeDateTime):
                    row_group_.push_back(std::make_unique<DateTime>());
                    break;
                case static_cast<int64_t>(Types::TypeTimestamp):
                    row_group_.push_back(std::make_unique<Timestamp>());
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
        std::vector<uint8_t> scheme_bytes = scheme_.Serialize();
        output_.write(reinterpret_cast<const char*>(file_metadata.data()), file_metadata.size() * sizeof(int64_t));
        output_.write(reinterpret_cast<const char*>(scheme_bytes.data()), scheme_bytes.size());
        uint64_t metadata_size = scheme_bytes.size() + file_metadata.size() * sizeof(int64_t);
        output_.write(reinterpret_cast<const char*>(&metadata_size), sizeof(metadata_size));
        csv_reader_.Close();
    }

protected:
    void WriteGroup() {
        int64_t group_capacity = 0;
        int64_t row_count = 0;
        std::vector<std::vector<std::string>> str_batch(column_num_);
        while (group_capacity <= RowGroupSize && !csv_reader_.IsEnd()) {
            std::vector<std::string> row = csv_reader_.GetNextLineAndSplitIntoTokens();
            if (row.size() == 0) {
                break;
            }
            for (int64_t i = 0; i < column_num_; ++i) {
                str_batch[i].push_back(row[i]);
                switch (types_[i]) {
                    case static_cast<int64_t>(Types::TypeDouble):
                        group_capacity += sizeof(double);
                        break;
                    case static_cast<int64_t>(Types::TypeInt16):
                        group_capacity += sizeof(int16_t);
                        break;
                    case static_cast<int64_t>(Types::TypeInt32):
                        group_capacity += sizeof(int32_t);
                        break;
                    case static_cast<int64_t>(Types::TypeInt64):
                        group_capacity += sizeof(int64_t);
                        break;
                    case static_cast<int64_t>(Types::TypeDateTime):
                    case static_cast<int64_t>(Types::TypeTimestamp):
                        group_capacity += sizeof(uint32_t);
                        break;
                    case static_cast<int64_t>(Types::TypeString):
                        group_capacity += sizeof(char) * row[i].size() + sizeof(int64_t);
                        break;
                }
            }
            ++row_count;
        }
        for (int64_t i = 0; i < column_num_; ++i) {
            row_group_[i]->AddColumn(str_batch[i]);
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
    Scheme& scheme_;
    std::vector<std::unique_ptr<Column>> row_group_;
    std::vector<int64_t> all_batch_metadata_;
    std::vector<int64_t> types_;

};

void RowGroupWriter::WriteAll() {
    impl_->WriteAll();
}

RowGroupWriter::~RowGroupWriter() = default;
