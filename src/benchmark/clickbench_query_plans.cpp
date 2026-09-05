#include "clickbench_query_plans.h"

#include "../column_types/column_types.h"
#include "../file_reader/file_reader.h"
#include "../scheme/scheme.h"
#include "../operators/operators.h"
#include "../utilities/utilities.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

void RequireQueryResult(bool condition, const char* expression) {
    if (!condition) {
        throw std::runtime_error(
            std::string("ClickBench query produced an invalid result: ") + expression
        );
    }
}

#define CHECK_TRUE(expression) \
    RequireQueryResult(static_cast<bool>(expression), #expression)

Scheme GetDbScheme(const char* input_db_file) {
    std::ifstream input(input_db_file, std::ios::binary | std::ios::ate);
    RowGroupReader reader(input);
    return reader.GetScheme();
}

std::string ExtractRefererHost(const std::string& referer) {
    std::string_view host = referer;
    constexpr std::string_view http_prefix = "http://";
    constexpr std::string_view https_prefix = "https://";
    if (host.starts_with(http_prefix)) {
        host.remove_prefix(http_prefix.size());
    } else if (host.starts_with(https_prefix)) {
        host.remove_prefix(https_prefix.size());
    }
    constexpr std::string_view www_prefix = "www.";
    if (host.starts_with(www_prefix)) {
        host.remove_prefix(www_prefix.size());
    }
    size_t slash_pos = host.find('/');
    if (slash_pos != std::string_view::npos) {
        host = host.substr(0, slash_pos);
    }
    return std::string(host);
}

std::string TruncateToMinute(const std::string& event_time) {
    if (event_time.size() < 16) {
        return event_time;
    }
    return event_time.substr(0, 16) + ":00";
}

void Query2(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"AdvEngineID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "AdvEngineID";
    int64_t filter_value = 0;
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<int64_t>>(filter_column, CompareFilter<int64_t>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> aggr_cols{"AdvEngineID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(filter_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
}

void Query3(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"AdvEngineID", "ResolutionWidth"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"AdvEngineID", "AdvEngineID", "ResolutionWidth"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::SUM,
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::AVG
    };
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
    output << batch.value()[1]->GetCellAsString(0) << std::endl;
    output << batch.value()[2]->GetCellAsString(0) << std::endl;
}

void Query4(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::AVG};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
}

void Query5(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
}

void Query6(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
}

void Query7(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"EventDate"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"EventDate", "EventDate"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::MIN,
        GlobalAggregationOperator::Op::MAX
    };
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
    output << batch.value()[1]->GetCellAsString(0) << std::endl;
}

void Query8(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"AdvEngineID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "AdvEngineID";
    int64_t filter_value = 0;
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<int64_t>>(filter_column, CompareFilter<int64_t>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"AdvEngineID"};
    std::vector<std::string> aggr_cols{"AdvEngineID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(group_by_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> adv_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < adv_engine_ids.size(); ++i) {
        output << adv_engine_ids[i] << "," << counts[i] << std::endl;
    }
}

void Query9(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"RegionID", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"RegionID"};
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> region_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < region_ids.size(); ++i) {
        output << region_ids[i] << "," << user_counts[i] << std::endl;
    }
}

void Query10(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"RegionID", "AdvEngineID", "ResolutionWidth", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"RegionID"};
    std::vector<std::string> aggr_cols{"AdvEngineID", "AdvEngineID", "ResolutionWidth", "UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::SUM,
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::AVG,
        GlobalAggregationOperator::Op::CountDistinct
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> region_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> adv_engine_sums = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> distinct_user_counts = batch.value()[4]->GetColumnAsString();
    for (int64_t i = 0; i < region_ids.size(); ++i) {
        output << region_ids[i] << ","
                  << adv_engine_sums[i] << ","
                  << counts[i] << ","
                  << resolution_width_avg[i] << ","
                  << distinct_user_counts[i] << std::endl;
    }
}

void Query11(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"MobilePhoneModel", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "MobilePhoneModel";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"MobilePhoneModel"};
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> mobile_phone_models = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < mobile_phone_models.size(); ++i) {
        output << mobile_phone_models[i] << "," << user_counts[i] << std::endl;
    }
}

void Query12(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"MobilePhone", "MobilePhoneModel", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "MobilePhoneModel";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"MobilePhone", "MobilePhoneModel"};
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> mobile_phones = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> mobile_phone_models = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[2]->GetColumnAsString();
    for (int64_t i = 0; i < mobile_phones.size(); ++i) {
        output << mobile_phones[i] << ","
                  << mobile_phone_models[i] << ","
                  << user_counts[i] << std::endl;
    }
}

void Query13(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        output << search_phrases[i] << "," << counts[i] << std::endl;
    }
}

void Query14(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchPhrase"};
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        output << search_phrases[i] << "," << user_counts[i] << std::endl;
    }
}

void Query15(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchEngineID", "SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchEngineID", "SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    for (int64_t i = 0; i < search_engine_ids.size(); ++i) {
        output << search_engine_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query16(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"UserID"};
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        output << user_ids[i] << "," << counts[i] << std::endl;
    }
}

void Query17(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID", "SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"UserID", "SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        output << user_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query18(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID", "SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"UserID", "SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::optional<Batch> batch = group_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    int64_t limit = std::min<int64_t>(10, user_ids.size());
    for (int64_t i = 0; i < limit; ++i) {
        output << user_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query19(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID", "EventTime", "SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"UserID", "EventTime", "SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};

    std::vector<AggregationTransform> group_by_transforms(3);
    group_by_transforms[1].fn = [](const CellTypes& value) -> CellTypes {
        const std::string& event_time = std::get<std::string>(value);
        if (event_time.size() < 16) {
            return static_cast<int64_t>(0);
        }
        return static_cast<int64_t>(std::stoll(event_time.substr(14, 2)));
    };
    group_by_transforms[1].output_type = static_cast<int64_t>(Types::TypeInt64);

    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(
        std::move(scan_operator),
        group_by_fields,
        aggr_cols,
        aggr_op,
        scheme,
        std::vector<AggregationTransform>{},
        group_by_transforms
    );
    std::vector<int> order_by_ids{3};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator =
        std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> minutes = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[3]->GetColumnAsString();
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        output << user_ids[i] << ","
                  << minutes[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query20(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "UserID";
    int64_t filter_value = 435090932899640449;
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<int64_t>>(filter_column, CompareFilter<int64_t>::Op::EQ, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    int64_t user_id_col_id = scheme.GetColumnIndex("UserID");
    int64_t row_count = 0;
    while (std::optional<Batch> batch = filter_operator->Next()) {
        std::vector<std::string> user_ids = batch.value()[user_id_col_id]->GetColumnAsString();
        row_count += user_ids.size();
        for (const auto& user_id : user_ids) {
            output << user_id << std::endl;
        }
    }
}

void Query21(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<LikeFilter>("URL", "google", scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(filter_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    CHECK_TRUE(batch.has_value());
    output << batch.value()[0]->GetCellAsString(0) << std::endl;
}

void Query22(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase", "URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<LikeFilter>("URL", "google", scheme),
        std::make_unique<CompareFilter<std::string>>("SearchPhrase", CompareFilter<std::string>::Op::NE, std::string(""), scheme)
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchPhrase"};
    std::vector<std::string> aggr_cols{"URL", "SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::MIN,
        GlobalAggregationOperator::Op::COUNT
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> min_urls = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        output << search_phrases[i] << ","
                  << min_urls[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query23(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase", "URL", "Title", "UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<LikeFilter>("Title", "Google", scheme),
        std::make_unique<AndFilter>(
            std::make_unique<NotFilter>(std::make_unique<LikeFilter>("URL", ".google.", scheme)),
            std::make_unique<CompareFilter<std::string>>("SearchPhrase", CompareFilter<std::string>::Op::NE, std::string(""), scheme)
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchPhrase"};
    std::vector<std::string> aggr_cols{"URL", "Title", "SearchPhrase", "UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::MIN,
        GlobalAggregationOperator::Op::MIN,
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::CountDistinct
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{3};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> min_urls = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> min_titles = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> distinct_user_counts = batch.value()[4]->GetColumnAsString();
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        output << search_phrases[i] << ","
                  << min_urls[i] << ","
                  << min_titles[i] << ","
                  << counts[i] << ","
                  << distinct_user_counts[i] << std::endl;
    }
}

void Query24(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns = scheme.GetNamesOrdered();
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<LikeFilter>("URL", "google", scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<int> order_by_ids{scheme.GetColumnIndex("EventTime")};
    bool is_desc = false;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(filter_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    int64_t row_count = batch.value()[0]->GetRowCount();
    for (int64_t r = 0; r < row_count; ++r) {
        for (int64_t c = 0; c < batch.value().size(); ++c) {
            output << batch.value()[c]->GetCellAsString(r);
            if (c + 1 != batch.value().size()) {
                output << ",";
            }
        }
        output << std::endl;
    }
}

void Query25(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase", "EventTime"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<int> order_by_ids{1};
    bool is_desc = false;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(filter_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    for (const auto& search_phrase : search_phrases) {
        output << search_phrase << std::endl;
    }
}

void Query26(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<int> order_by_ids{0};
    bool is_desc = false;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(filter_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    for (const auto& search_phrase : search_phrases) {
        output << search_phrase << std::endl;
    }
}

void Query27(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase", "EventTime"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<int> order_by_ids{1, 0};
    bool is_desc = false;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(filter_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    for (const auto& search_phrase : search_phrases) {
        output << search_phrase << std::endl;
    }
}

void Query28(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> where_condition =
        std::make_unique<CompareFilter<std::string>>("URL", CompareFilter<std::string>::Op::NE, std::string(""), scheme);
    std::unique_ptr<IOperator> where_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(where_condition));

    std::vector<std::string> group_by_fields{"CounterID"};
    std::vector<std::string> aggr_cols{"URL", "URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::AVG,
        GlobalAggregationOperator::Op::COUNT
    };
    AggregationTransform url_length_transform;
    url_length_transform.fn = [](const CellTypes& value) -> CellTypes {
        return static_cast<int64_t>(std::get<std::string>(value).size());
    };
    url_length_transform.output_type = static_cast<int64_t>(Types::TypeInt64);
    std::vector<AggregationTransform> transforms{
        url_length_transform,
        {}
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(
        std::move(where_operator),
        group_by_fields,
        aggr_cols,
        aggr_op,
        scheme,
        transforms
    );

    std::unique_ptr<FilterCondition> having_condition =
        std::make_unique<CompareFilterByIndex>(2, Column::Op::GT, static_cast<int64_t>(100000));
    std::unique_ptr<IOperator> having_operator = std::make_unique<FilterOperator>(std::move(group_by_operator), std::move(having_condition));

    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 25;
    std::unique_ptr<IOperator> order_by_limit_operator =
        std::make_unique<OrderByLimitKOperator>(std::move(having_operator), limit, is_desc, order_by_ids, scheme);

    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> counter_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> avg_url_lengths = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    for (int64_t i = 0; i < counter_ids.size(); ++i) {
        output << counter_ids[i] << ","
                  << avg_url_lengths[i] << ","
                  << counts[i] << std::endl;
    }
}

void Query29(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"Referer"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> where_condition =
        std::make_unique<CompareFilter<std::string>>("Referer", CompareFilter<std::string>::Op::NE, std::string(""), scheme);
    std::unique_ptr<IOperator> where_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(where_condition));

    std::vector<std::string> group_by_fields{"Referer"};
    std::vector<std::string> aggr_cols{"Referer", "Referer", "Referer"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::AVG,
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::MIN
    };

    AggregationTransform referer_length_transform;
    referer_length_transform.fn = [](const CellTypes& value) -> CellTypes {
        return static_cast<int64_t>(std::get<std::string>(value).size());
    };
    referer_length_transform.output_type = static_cast<int64_t>(Types::TypeInt64);

    AggregationTransform referer_host_transform;
    referer_host_transform.fn = [](const CellTypes& value) -> CellTypes {
        return ExtractRefererHost(std::get<std::string>(value));
    };
    referer_host_transform.output_type = static_cast<int64_t>(Types::TypeString);

    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(
        std::move(where_operator),
        group_by_fields,
        aggr_cols,
        aggr_op,
        scheme,
        std::vector<AggregationTransform>{referer_length_transform, {}, {}},
        std::vector<AggregationTransform>{referer_host_transform}
    );

    std::unique_ptr<FilterCondition> having_condition =
        std::make_unique<CompareFilterByIndex>(2, Column::Op::GT, static_cast<int64_t>(100000));
    std::unique_ptr<IOperator> having_operator = std::make_unique<FilterOperator>(std::move(group_by_operator), std::move(having_condition));

    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 25;
    std::unique_ptr<IOperator> order_by_limit_operator =
        std::make_unique<OrderByLimitKOperator>(std::move(having_operator), limit, is_desc, order_by_ids, scheme);

    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> hosts = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> avg_lengths = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> min_referers = batch.value()[3]->GetColumnAsString();
    for (int64_t i = 0; i < hosts.size(); ++i) {
        output << hosts[i] << ","
                  << avg_lengths[i] << ","
                  << counts[i] << ","
                  << min_referers[i] << std::endl;
    }
}

void Query30(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);

    constexpr int kNumAggregations = 90;
    std::vector<std::string> columns(kNumAggregations, "ResolutionWidth");
    std::vector<GlobalAggregationOperator::Op> aggr_ops(kNumAggregations, GlobalAggregationOperator::Op::SUM);
    std::vector<AggregationTransform> transforms;
    transforms.reserve(kNumAggregations);

    for (int offset = 0; offset < kNumAggregations; ++offset) {
        AggregationTransform transform;
        transform.fn = [offset](const CellTypes& value) -> CellTypes {
            return std::get<int64_t>(value) + static_cast<int64_t>(offset);
        };
        transform.output_type = static_cast<int64_t>(Types::TypeInt64);
        transforms.push_back(std::move(transform));
    }

    std::unique_ptr<IOperator> scan_operator =
        std::make_unique<ScanOperator>(input_db_file, std::vector<std::string>{"ResolutionWidth"});
    std::unique_ptr<IOperator> aggregation_operator =
        std::make_unique<GlobalAggregationOperator>(columns, std::move(scan_operator), aggr_ops, scheme, transforms);

    std::optional<Batch> batch = aggregation_operator->Next();
    CHECK_TRUE(batch.has_value());

    for (int i = 0; i < kNumAggregations; ++i) {
        output << batch.value()[i]->GetCellAsString(0);
        if (i + 1 != kNumAggregations) {
            output << ",";
        }
    }
    output << std::endl;
}

void Query31(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchEngineID", "ClientIP", "SearchPhrase", "IsRefresh", "ResolutionWidth"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"SearchEngineID", "ClientIP"};
    std::vector<std::string> aggr_cols{"SearchPhrase", "IsRefresh", "ResolutionWidth"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::SUM,
        GlobalAggregationOperator::Op::AVG
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> search_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    for (int64_t i = 0; i < search_engine_ids.size(); ++i) {
        output << search_engine_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

void Query32(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"WatchID", "ClientIP", "SearchPhrase", "IsRefresh", "ResolutionWidth"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "SearchPhrase";
    std::string filter_value = "";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::NE, filter_value, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"WatchID", "ClientIP"};
    std::vector<std::string> aggr_cols{"SearchPhrase", "IsRefresh", "ResolutionWidth"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::SUM,
        GlobalAggregationOperator::Op::AVG
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> watch_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    for (int64_t i = 0; i < watch_ids.size(); ++i) {
        output << watch_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

void Query33(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"WatchID", "ClientIP", "IsRefresh", "ResolutionWidth"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"WatchID", "ClientIP"};
    std::vector<std::string> aggr_cols{"WatchID", "IsRefresh", "ResolutionWidth"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{
        GlobalAggregationOperator::Op::COUNT,
        GlobalAggregationOperator::Op::SUM,
        GlobalAggregationOperator::Op::AVG
    };
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> watch_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    for (int64_t i = 0; i < watch_ids.size(); ++i) {
        output << watch_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

void Query34(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"URL"};
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < urls.size(); ++i) {
        output << urls[i] << "," << counts[i] << std::endl;
    }
}

void Query35(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"URL"};
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < urls.size(); ++i) {
        output << 1 << "," << urls[i] << "," << counts[i] << std::endl;
    }
}

void Query36(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"ClientIP"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"ClientIP"};
    std::vector<std::string> aggr_cols{"ClientIP"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator =
        std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator =
        std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> client_ips = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < client_ips.size(); ++i) {
        int64_t client_ip = std::stoll(client_ips[i]);
        output << client_ip << ","
                  << client_ip - 1 << ","
                  << client_ip - 2 << ","
                  << client_ip - 3 << ","
                  << counts[i] << std::endl;
    }
}

void Query37(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "DontCountHits", "IsRefresh", "URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-01"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-31"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("DontCountHits", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<AndFilter>(
                        std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                        std::make_unique<CompareFilter<std::string>>("URL", CompareFilter<std::string>::Op::NE, std::string(""), scheme)
                    )
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"URL"};
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < urls.size(); ++i) {
        output << urls[i] << "," << page_views[i] << std::endl;
    }
}

void Query38(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "DontCountHits", "IsRefresh", "Title"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-01"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-31"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("DontCountHits", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<AndFilter>(
                        std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                        std::make_unique<CompareFilter<std::string>>("Title", CompareFilter<std::string>::Op::NE, std::string(""), scheme)
                    )
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"Title"};
    std::vector<std::string> aggr_cols{"Title"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    int limit = 10;
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(group_by_operator), limit, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> titles = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < titles.size(); ++i) {
        output << titles[i] << "," << page_views[i] << std::endl;
    }
}

void Query39(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "IsRefresh", "IsLink", "IsDownload", "URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-01"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-31"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<AndFilter>(
                        std::make_unique<CompareFilter<int64_t>>("IsLink", CompareFilter<int64_t>::Op::NE, static_cast<int64_t>(0), scheme),
                        std::make_unique<CompareFilter<int64_t>>("IsDownload", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme)
                    )
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"URL"};
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{1};
    bool is_desc = true;
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(group_by_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    int64_t offset = 1000;
    int64_t limit = 10;
    const std::size_t begin = std::min<std::size_t>(offset, urls.size());
    const std::size_t end = std::min<std::size_t>(begin + limit, urls.size());
    for (std::size_t i = begin; i < end; ++i) {
        output << urls[i] << "," << page_views[i] << std::endl;
    }
}

void Query41(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "IsRefresh", "TraficSourceID", "RefererHash", "URLHash"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    int64_t referer_hash = 3594120000172545465;
    std::unique_ptr<FilterCondition> trafic_source_condition = std::make_unique<OrFilter>(
        std::make_unique<CompareFilter<int64_t>>("TraficSourceID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(-1), scheme),
        std::make_unique<CompareFilter<int64_t>>("TraficSourceID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(6), scheme)
    );
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-01"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-31"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<AndFilter>(
                        std::move(trafic_source_condition),
                        std::make_unique<CompareFilter<int64_t>>("RefererHash", CompareFilter<int64_t>::Op::EQ, referer_hash, scheme)
                    )
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"URLHash", "EventDate"};
    std::vector<std::string> aggr_cols{"URLHash"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(group_by_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> url_hashes = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> event_dates = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[2]->GetColumnAsString();
    int64_t offset = 100;
    int64_t limit = 10;
    const std::size_t begin = std::min<std::size_t>(offset, url_hashes.size());
    const std::size_t end = std::min<std::size_t>(begin + limit, url_hashes.size());
    for (std::size_t i = begin; i < end; ++i) {
        output << url_hashes[i] << ","
                  << event_dates[i] << ","
                  << page_views[i] << std::endl;
    }
}

void Query42(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "IsRefresh", "DontCountHits", "URLHash", "WindowClientWidth", "WindowClientHeight"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    int64_t url_hash = 2868770270353813622;
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-01"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-31"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<AndFilter>(
                        std::make_unique<CompareFilter<int64_t>>("DontCountHits", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                        std::make_unique<CompareFilter<int64_t>>("URLHash", CompareFilter<int64_t>::Op::EQ, url_hash, scheme)
                    )
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> group_by_fields{"WindowClientWidth", "WindowClientHeight"};
    std::vector<std::string> aggr_cols{"WindowClientWidth"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(filter_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::vector<int> order_by_ids{2};
    bool is_desc = true;
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(group_by_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> window_client_widths = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> window_client_heights = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[2]->GetColumnAsString();
    int64_t offset = 10000;
    int64_t limit = 10;
    const std::size_t begin =
        std::min<std::size_t>(offset, window_client_widths.size());
    const std::size_t end =
        std::min<std::size_t>(begin + limit, window_client_widths.size());
    for (std::size_t i = begin; i < end; ++i) {
        output << window_client_widths[i] << ","
                  << window_client_heights[i] << ","
                  << page_views[i] << std::endl;
    }
}

void Query43(const char* input_db_file, std::ostream& output) {
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"CounterID", "EventDate", "IsRefresh", "DontCountHits", "EventTime"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<AndFilter>(
        std::make_unique<CompareFilter<int64_t>>("CounterID", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(62), scheme),
        std::make_unique<AndFilter>(
            std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::GE, std::string("2013-07-14"), scheme),
            std::make_unique<AndFilter>(
                std::make_unique<CompareFilter<std::string>>("EventDate", CompareFilter<std::string>::Op::LE, std::string("2013-07-15"), scheme),
                std::make_unique<AndFilter>(
                    std::make_unique<CompareFilter<int64_t>>("IsRefresh", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme),
                    std::make_unique<CompareFilter<int64_t>>("DontCountHits", CompareFilter<int64_t>::Op::EQ, static_cast<int64_t>(0), scheme)
                )
            )
        )
    );
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));

    AggregationTransform minute_trunc_transform;
    minute_trunc_transform.fn = [](const CellTypes& value) -> CellTypes {
        return TruncateToMinute(std::get<std::string>(value));
    };
    minute_trunc_transform.output_type = static_cast<int64_t>(Types::TypeString);

    std::vector<std::string> group_by_fields{"EventTime"};
    std::vector<std::string> aggr_cols{"EventTime"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(
        std::move(filter_operator),
        group_by_fields,
        aggr_cols,
        aggr_op,
        scheme,
        std::vector<AggregationTransform>{},
        std::vector<AggregationTransform>{minute_trunc_transform}
    );

    std::vector<int> order_by_ids{0};
    bool is_desc = false;
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(group_by_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    CHECK_TRUE(batch.has_value());
    std::vector<std::string> minutes = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    int64_t offset = 1000;
    int64_t limit = 10;
    const std::size_t begin = std::min<std::size_t>(offset, minutes.size());
    const std::size_t end = std::min<std::size_t>(begin + limit, minutes.size());
    for (std::size_t i = begin; i < end; ++i) {
        output << minutes[i] << ","
                  << page_views[i] << std::endl;
    }
}

std::vector<std::vector<std::string>> ParseOutputRows(const std::string& output) {
    std::vector<std::vector<std::string>> rows;
    std::istringstream input(output);
    for (std::string line; std::getline(input, line);) {
        if (!line.empty()) {
            rows.push_back({std::move(line)});
        }
    }
    return rows;
}

} // namespace

namespace benchmark_app {

ClickBenchQueryOutput RunClickBenchQueryPlan(
    int query_id,
    const std::filesystem::path& dataset_path
) {
    if (query_id == 40) {
        return {{"status"}, {{"not_implemented"}}};
    }

    const std::string input_db_file = dataset_path.string();
    std::ostringstream output;

    switch (query_id) {
        case 2: Query2(input_db_file.c_str(), output); break;
        case 3: Query3(input_db_file.c_str(), output); break;
        case 4: Query4(input_db_file.c_str(), output); break;
        case 5: Query5(input_db_file.c_str(), output); break;
        case 6: Query6(input_db_file.c_str(), output); break;
        case 7: Query7(input_db_file.c_str(), output); break;
        case 8: Query8(input_db_file.c_str(), output); break;
        case 9: Query9(input_db_file.c_str(), output); break;
        case 10: Query10(input_db_file.c_str(), output); break;
        case 11: Query11(input_db_file.c_str(), output); break;
        case 12: Query12(input_db_file.c_str(), output); break;
        case 13: Query13(input_db_file.c_str(), output); break;
        case 14: Query14(input_db_file.c_str(), output); break;
        case 15: Query15(input_db_file.c_str(), output); break;
        case 16: Query16(input_db_file.c_str(), output); break;
        case 17: Query17(input_db_file.c_str(), output); break;
        case 18: Query18(input_db_file.c_str(), output); break;
        case 19: Query19(input_db_file.c_str(), output); break;
        case 20: Query20(input_db_file.c_str(), output); break;
        case 21: Query21(input_db_file.c_str(), output); break;
        case 22: Query22(input_db_file.c_str(), output); break;
        case 23: Query23(input_db_file.c_str(), output); break;
        case 24: Query24(input_db_file.c_str(), output); break;
        case 25: Query25(input_db_file.c_str(), output); break;
        case 26: Query26(input_db_file.c_str(), output); break;
        case 27: Query27(input_db_file.c_str(), output); break;
        case 28: Query28(input_db_file.c_str(), output); break;
        case 29: Query29(input_db_file.c_str(), output); break;
        case 30: Query30(input_db_file.c_str(), output); break;
        case 31: Query31(input_db_file.c_str(), output); break;
        case 32: Query32(input_db_file.c_str(), output); break;
        case 33: Query33(input_db_file.c_str(), output); break;
        case 34: Query34(input_db_file.c_str(), output); break;
        case 35: Query35(input_db_file.c_str(), output); break;
        case 36: Query36(input_db_file.c_str(), output); break;
        case 37: Query37(input_db_file.c_str(), output); break;
        case 38: Query38(input_db_file.c_str(), output); break;
        case 39: Query39(input_db_file.c_str(), output); break;
        case 41: Query41(input_db_file.c_str(), output); break;
        case 42: Query42(input_db_file.c_str(), output); break;
        case 43: Query43(input_db_file.c_str(), output); break;
        default:
            throw std::invalid_argument(
                "Unsupported ClickBench query id: " + std::to_string(query_id)
            );
    }

    return {{"clickbench_output"}, ParseOutputRows(output.str())};
}

} // namespace benchmark_app
