#include <gtest/gtest.h>

#include "src/csv_wrapper/csv_wrapper.h"
#include "src/column_types/column_types.h"
#include "src/file_writer/file_writer.h"
#include "src/file_reader/file_reader.h"
#include "src/scheme/scheme.h"
#include "src/operators/operators.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include <random>

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

TEST(ClickBenchQueriesTest, Query01CountAll) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"WatchID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"WatchID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query02CountWhereAdvEngineIdNotZero) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query03SumCountAvg) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    std::cout << batch.value()[1]->GetCellAsString(0) << std::endl;
    std::cout << batch.value()[2]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
    EXPECT_EQ(batch.value()[1]->GetColumnAsString().size(), 1);
    EXPECT_EQ(batch.value()[2]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query04AvgUserId) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::AVG};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query05CountDistinctUserId) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"UserID"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query06CountDistinctSearchPhrase) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query07MinMaxEventDate) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    std::cout << batch.value()[1]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
    EXPECT_EQ(batch.value()[1]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query08GroupByAdvEngineIdOrderByCountDesc) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> adv_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    for (int64_t i = 0; i < adv_engine_ids.size(); ++i) {
        std::cout << adv_engine_ids[i] << "," << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query09GroupByRegionIdCountDistinctUserIdOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> region_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(region_ids.size(), 10);
    EXPECT_EQ(user_counts.size(), 10);
    for (int64_t i = 0; i < region_ids.size(); ++i) {
        std::cout << region_ids[i] << "," << user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query10GroupByRegionIdMultipleAggregationsOrderByCountDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 5);
    std::vector<std::string> region_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> adv_engine_sums = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> distinct_user_counts = batch.value()[4]->GetColumnAsString();
    EXPECT_EQ(region_ids.size(), 10);
    EXPECT_EQ(adv_engine_sums.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    EXPECT_EQ(resolution_width_avg.size(), 10);
    EXPECT_EQ(distinct_user_counts.size(), 10);
    for (int64_t i = 0; i < region_ids.size(); ++i) {
        std::cout << region_ids[i] << ","
                  << adv_engine_sums[i] << ","
                  << counts[i] << ","
                  << resolution_width_avg[i] << ","
                  << distinct_user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query11GroupByMobilePhoneModelCountDistinctUserIdOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> mobile_phone_models = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(mobile_phone_models.size(), 10);
    EXPECT_EQ(user_counts.size(), 10);
    for (int64_t i = 0; i < mobile_phone_models.size(); ++i) {
        std::cout << mobile_phone_models[i] << "," << user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query12GroupByMobilePhoneAndModelCountDistinctUserIdOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> mobile_phones = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> mobile_phone_models = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(mobile_phones.size(), 10);
    EXPECT_EQ(mobile_phone_models.size(), 10);
    EXPECT_EQ(user_counts.size(), 10);
    for (int64_t i = 0; i < mobile_phones.size(); ++i) {
        std::cout << mobile_phones[i] << ","
                  << mobile_phone_models[i] << ","
                  << user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query13GroupBySearchPhraseCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        std::cout << search_phrases[i] << "," << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query14GroupBySearchPhraseCountDistinctUserIdOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> user_counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(user_counts.size(), 10);
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        std::cout << search_phrases[i] << "," << user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query15GroupBySearchEngineIdAndSearchPhraseCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> search_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(search_engine_ids.size(), 10);
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < search_engine_ids.size(); ++i) {
        std::cout << search_engine_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query16GroupByUserIdCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(user_ids.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        std::cout << user_ids[i] << "," << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query17GroupByUserIdAndSearchPhraseCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(user_ids.size(), 10);
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        std::cout << user_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query18GroupByUserIdAndSearchPhraseLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"UserID", "SearchPhrase"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> group_by_fields{"UserID", "SearchPhrase"};
    std::vector<std::string> aggr_cols{"SearchPhrase"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::optional<Batch> batch = group_by_operator->Next();
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), user_ids.size());
    EXPECT_EQ(counts.size(), user_ids.size());
    int64_t limit = std::min<int64_t>(10, user_ids.size());
    for (int64_t i = 0; i < limit; ++i) {
        std::cout << user_ids[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query19GroupByUserIdMinuteAndSearchPhraseCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 4);
    std::vector<std::string> user_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> minutes = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> search_phrases = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[3]->GetColumnAsString();
    EXPECT_EQ(user_ids.size(), 10);
    EXPECT_EQ(minutes.size(), 10);
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < user_ids.size(); ++i) {
        std::cout << user_ids[i] << ","
                  << minutes[i] << ","
                  << search_phrases[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query20FilterUserId) {
    const char* input_db_file = "db_file_benchmark.egg";
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
            std::cout << user_id << std::endl;
        }
    }
    EXPECT_GT(row_count, 0);
}

TEST(ClickBenchQueriesTest, Query21CountWhereUrlLikeGoogle) {
    const char* input_db_file = "db_file_benchmark.egg";
    Scheme scheme = GetDbScheme(input_db_file);
    std::vector<std::string> columns{"URL"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::unique_ptr<FilterCondition> condition = std::make_unique<LikeFilter>("URL", "google", scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::vector<std::string> aggr_cols{"URL"};
    std::vector<GlobalAggregationOperator::Op> aggr_op{GlobalAggregationOperator::Op::COUNT};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(filter_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    ASSERT_TRUE(batch.has_value());
    std::cout << batch.value()[0]->GetCellAsString(0) << std::endl;
    EXPECT_EQ(batch.value()[0]->GetColumnAsString().size(), 1);
}

TEST(ClickBenchQueriesTest, Query22GroupBySearchPhraseMinUrlCountWhereUrlLikeGoogleOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> min_urls = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(min_urls.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        std::cout << search_phrases[i] << ","
                  << min_urls[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query23GroupBySearchPhraseMinUrlMinTitleCountCountDistinctUserIdWithLikeFiltersOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 5);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> min_urls = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> min_titles = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> distinct_user_counts = batch.value()[4]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    EXPECT_EQ(min_urls.size(), 10);
    EXPECT_EQ(min_titles.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    EXPECT_EQ(distinct_user_counts.size(), 10);
    for (int64_t i = 0; i < search_phrases.size(); ++i) {
        std::cout << search_phrases[i] << ","
                  << min_urls[i] << ","
                  << min_titles[i] << ","
                  << counts[i] << ","
                  << distinct_user_counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query24SelectAllWhereUrlLikeGoogleOrderByEventTimeLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), columns.size());
    int64_t row_count = batch.value()[0]->GetRowCount();
    EXPECT_EQ(row_count, 10);
    for (int64_t r = 0; r < row_count; ++r) {
        for (int64_t c = 0; c < batch.value().size(); ++c) {
            std::cout << batch.value()[c]->GetCellAsString(r);
            if (c + 1 != batch.value().size()) {
                std::cout << ",";
            }
        }
        std::cout << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query25SearchPhraseWhereNotEmptyOrderByEventTimeLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    for (const auto& search_phrase : search_phrases) {
        std::cout << search_phrase << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query26SearchPhraseWhereNotEmptyOrderBySearchPhraseLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 1);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    for (const auto& search_phrase : search_phrases) {
        std::cout << search_phrase << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query27SearchPhraseWhereNotEmptyOrderByEventTimeAndSearchPhraseLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> search_phrases = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(search_phrases.size(), 10);
    for (const auto& search_phrase : search_phrases) {
        std::cout << search_phrase << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query28GroupByCounterIdAvgLengthUrlCountHavingOrderByDescLimit25) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> counter_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> avg_url_lengths = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(counter_ids.size(), 25);
    EXPECT_EQ(avg_url_lengths.size(), 25);
    EXPECT_EQ(counts.size(), 25);
    for (int64_t i = 0; i < counter_ids.size(); ++i) {
        std::cout << counter_ids[i] << ","
                  << avg_url_lengths[i] << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query29GroupByRefererHostAvgLengthCountMinHavingOrderByDescLimit25) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 4);
    std::vector<std::string> hosts = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> avg_lengths = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> min_referers = batch.value()[3]->GetColumnAsString();
    EXPECT_EQ(hosts.size(), 25);
    EXPECT_EQ(avg_lengths.size(), 25);
    EXPECT_EQ(counts.size(), 25);
    EXPECT_EQ(min_referers.size(), 25);
    for (int64_t i = 0; i < hosts.size(); ++i) {
        std::cout << hosts[i] << ","
                  << avg_lengths[i] << ","
                  << counts[i] << ","
                  << min_referers[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query30ManySumsOfResolutionWidthWithOffsets) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), kNumAggregations);

    for (int i = 0; i < kNumAggregations; ++i) {
        EXPECT_EQ(batch.value()[i]->GetRowCount(), 1);
        std::cout << batch.value()[i]->GetCellAsString(0);
        if (i + 1 != kNumAggregations) {
            std::cout << ",";
        }
    }
    std::cout << std::endl;
}

TEST(ClickBenchQueriesTest, Query31GroupBySearchEngineIdAndClientIpCountSumAvgOrderByCountDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 5);
    std::vector<std::string> search_engine_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    EXPECT_EQ(search_engine_ids.size(), 10);
    EXPECT_EQ(client_ips.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    EXPECT_EQ(refresh_sums.size(), 10);
    EXPECT_EQ(resolution_width_avg.size(), 10);
    for (int64_t i = 0; i < search_engine_ids.size(); ++i) {
        std::cout << search_engine_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query32GroupByWatchIdAndClientIpCountSumAvgOrderByCountDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 5);
    std::vector<std::string> watch_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    EXPECT_EQ(watch_ids.size(), 10);
    EXPECT_EQ(client_ips.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    EXPECT_EQ(refresh_sums.size(), 10);
    EXPECT_EQ(resolution_width_avg.size(), 10);
    for (int64_t i = 0; i < watch_ids.size(); ++i) {
        std::cout << watch_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query33GroupByWatchIdAndClientIpCountSumAvgOrderByCountDescLimit10NoFilter) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 5);
    std::vector<std::string> watch_ids = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> client_ips = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[2]->GetColumnAsString();
    std::vector<std::string> refresh_sums = batch.value()[3]->GetColumnAsString();
    std::vector<std::string> resolution_width_avg = batch.value()[4]->GetColumnAsString();
    EXPECT_EQ(watch_ids.size(), 10);
    EXPECT_EQ(client_ips.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    EXPECT_EQ(refresh_sums.size(), 10);
    EXPECT_EQ(resolution_width_avg.size(), 10);
    for (int64_t i = 0; i < watch_ids.size(); ++i) {
        std::cout << watch_ids[i] << ","
                  << client_ips[i] << ","
                  << counts[i] << ","
                  << refresh_sums[i] << ","
                  << resolution_width_avg[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query34GroupByUrlCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(urls.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < urls.size(); ++i) {
        std::cout << urls[i] << "," << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query35GroupByConstantAndUrlCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(urls.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < urls.size(); ++i) {
        std::cout << 1 << "," << urls[i] << "," << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query36GroupByClientIpDerivedColumnsCountOrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> client_ips = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> counts = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(client_ips.size(), 10);
    EXPECT_EQ(counts.size(), 10);
    for (int64_t i = 0; i < client_ips.size(); ++i) {
        int64_t client_ip = std::stoll(client_ips[i]);
        std::cout << client_ip << ","
                  << client_ip - 1 << ","
                  << client_ip - 2 << ","
                  << client_ip - 3 << ","
                  << counts[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query37GroupByUrlCountFilteredJulyCounter62OrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(urls.size(), 10);
    EXPECT_EQ(page_views.size(), 10);
    for (int64_t i = 0; i < urls.size(); ++i) {
        std::cout << urls[i] << "," << page_views[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query38GroupByTitleCountFilteredJulyCounter62OrderByDescLimit10) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> titles = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(titles.size(), 10);
    EXPECT_EQ(page_views.size(), 10);
    for (int64_t i = 0; i < titles.size(); ++i) {
        std::cout << titles[i] << "," << page_views[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query39GroupByUrlCountFilteredJulyCounter62OrderByDescLimit10Offset1000) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> urls = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    int64_t offset = 1000;
    int64_t limit = 10;
    ASSERT_GE(urls.size(), offset + limit);
    ASSERT_GE(page_views.size(), offset + limit);
    for (int64_t i = offset; i < offset + limit; ++i) {
        std::cout << urls[i] << "," << page_views[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query41GroupByUrlHashAndEventDateCountFilteredJulyCounter62OrderByDescLimit10Offset100) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> url_hashes = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> event_dates = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[2]->GetColumnAsString();
    int64_t offset = 100;
    int64_t limit = 10;
    ASSERT_GE(url_hashes.size(), offset + limit);
    ASSERT_GE(event_dates.size(), offset + limit);
    ASSERT_GE(page_views.size(), offset + limit);
    for (int64_t i = offset; i < offset + limit; ++i) {
        std::cout << url_hashes[i] << ","
                  << event_dates[i] << ","
                  << page_views[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query42GroupByWindowClientWidthAndHeightCountFilteredJulyCounter62OrderByDescLimit10Offset10000) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 3);
    std::vector<std::string> window_client_widths = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> window_client_heights = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[2]->GetColumnAsString();
    int64_t offset = 10000;
    int64_t limit = 10;
    ASSERT_GE(window_client_widths.size(), offset + limit);
    ASSERT_GE(window_client_heights.size(), offset + limit);
    ASSERT_GE(page_views.size(), offset + limit);
    for (int64_t i = offset; i < offset + limit; ++i) {
        std::cout << window_client_widths[i] << ","
                  << window_client_heights[i] << ","
                  << page_views[i] << std::endl;
    }
}

TEST(ClickBenchQueriesTest, Query43GroupByDateTruncMinuteCountFilteredJulyCounter62OrderByAscLimit10Offset1000) {
    const char* input_db_file = "db_file_benchmark.egg";
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
    ASSERT_TRUE(batch.has_value());
    EXPECT_EQ(batch.value().size(), 2);
    std::vector<std::string> minutes = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> page_views = batch.value()[1]->GetColumnAsString();
    int64_t offset = 1000;
    int64_t limit = 10;
    ASSERT_GE(minutes.size(), offset + limit);
    ASSERT_GE(page_views.size(), offset + limit);
    for (int64_t i = offset; i < offset + limit; ++i) {
        std::cout << minutes[i] << ","
                  << page_views[i] << std::endl;
    }
}
