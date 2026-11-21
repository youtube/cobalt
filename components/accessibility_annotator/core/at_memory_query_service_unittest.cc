// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/at_memory_query_service.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_provider.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/at_memory_query_service_delegate.h"
#include "components/personal_context/core/context_memory_error.h"
#include "components/personal_context/core/personal_context_debug_features.h"
#include "net/base/mock_network_change_notifier.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::accessibility_annotator::MemoryDataType;

class MockAtMemoryQueryServiceDelegate : public AtMemoryQueryServiceDelegate {
 public:
  MOCK_METHOD(void,
              RetrieveLiveTabContext,
              (LiveTabContextQuery query,
               base::OnceCallback<void(LiveTabContextResponse)> callback),
              (override));
};

class FakeMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(const std::vector<MemoryDataType>& types,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    last_types_ = types;
    std::move(callback).Run(results_);
  }
  void SetResults(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }
  const std::vector<MemoryDataType>& last_types() const { return last_types_; }
  MemoryDataType last_type() const {
    return last_types_.empty() ? MemoryDataType::kUnknown : last_types_[0];
  }

 private:
  std::vector<MemorySearchResult> results_;
  std::vector<MemoryDataType> last_types_;
};

class FakePersonalContextResolver : public PersonalContextResolver {
 public:
  void Query(std::u16string query, QueryCallback callback) override {
    last_query_ = query;
    if (error_) {
      std::move(callback).Run(base::unexpected(*error_));
      return;
    }
    std::move(callback).Run(results_);
  }

  void set_results(std::vector<MemorySearchResult> results) {
    results_ = std::move(results);
  }

  void set_error(personal_context::ContextMemoryError error) {
    error_ = std::move(error);
  }

  std::u16string last_query() const { return last_query_; }

 private:
  std::u16string last_query_;
  std::vector<MemorySearchResult> results_;
  std::optional<personal_context::ContextMemoryError> error_;
};

class DelayedMemoryDataProvider : public MemoryDataProvider {
 public:
  void RetrieveAll(const std::vector<MemoryDataType>& types,
                   base::OnceCallback<void(std::vector<MemorySearchResult>)>
                       callback) override {
    callbacks_.push_back(std::move(callback));
  }
  void CompleteNext() {
    if (!callbacks_.empty()) {
      std::move(callbacks_.front()).Run({});
      callbacks_.erase(callbacks_.begin());
    }
  }

 private:
  std::vector<base::OnceCallback<void(std::vector<MemorySearchResult>)>>
      callbacks_;
};

class AtMemoryQueryServiceTest : public testing::Test {
 public:
  AtMemoryQueryServiceTest() = default;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the query service returns an internal failure status after
// shutdown.
TEST_F(AtMemoryQueryServiceTest, Query_AfterShutdown) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  service->Shutdown();

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
}

// Tests that the query service returns an internal failure status when no
// providers are available.
TEST_F(AtMemoryQueryServiceTest, Query_NoProviders) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      /*data_provider=*/nullptr, /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
}

// Tests that the query service returns a data fetch failure immediately when
// the network is offline.
TEST_F(AtMemoryQueryServiceTest, Query_Offline) {
  net::test::ScopedMockNetworkChangeNotifier notifier;
  notifier.mock_network_change_notifier()->SetConnectionType(
      net::NetworkChangeNotifier::CONNECTION_NONE);

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kNoConnectionFailure);
}

// Tests that the query service returns the expected results when the intent is
// successfully classified.
TEST_F(AtMemoryQueryServiceTest, Query_Success) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result(MemoryDataType::kNameFull, u"Name", u"John Doe");
  fake_data_provider->SetResults({result});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& search_results = future.Get();
  EXPECT_THAT(search_results.entries,
              testing::ElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kNameFull);
}

// Tests that the query service returns an empty list when the intent is
// unknown and there is no personal context resolver available.
TEST_F(AtMemoryQueryServiceTest,
       Query_UnknownIntent_NoPersonalContextResolver) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(result.status, MemorySearchStatus::kUnsupportedQuery);
}

// Tests that the query service returns unsupported query when the intent is
// unknown and the personal context resolver returns nothing.
TEST_F(AtMemoryQueryServiceTest,
       Query_UnknownIntent_PersonalContextResolverEmpty_ReturnsUnsupported) {
  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  fake_personal_context_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kUnsupportedQuery);
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(fake_personal_context_resolver->last_query(), u"random query");
}

// Tests that the query service queries the personal context resolver when the
// intent is unknown.
TEST_F(AtMemoryQueryServiceTest,
       Query_UnknownIntent_QueriesPersonalContextResolver) {
  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult personal_context_entry(
      MemoryDataType::kUnknown, u"Custom Type", u"Some Personal Context Value");
  fake_personal_context_resolver->set_results({personal_context_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries,
              testing::ElementsAre(testing::Field(
                  &MemorySearchResult::value, u"Some Personal Context Value")));
  EXPECT_EQ(fake_personal_context_resolver->last_query(), u"random query");
}

// Tests that the query service returns empty success when no local data is
// found for a known intent and there is no personal context resolver.
TEST_F(AtMemoryQueryServiceTest, Query_NoLocalData_NoPersonalContextResolver) {
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
}

// Tests that the query service queries the personal context resolver when no
// local data is found for a known intent.
TEST_F(AtMemoryQueryServiceTest,
       Query_NoLocalData_QueriesPersonalContextResolver) {
  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult personal_context_entry(MemoryDataType::kNameFull, u"Name",
                                            u"Jane Doe");
  fake_personal_context_resolver->set_results({personal_context_entry});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_personal_context_resolver->last_query(), u"what is my name");
}

// Tests that the query service returns success with an empty list when no local
// data is found and the personal context resolver also returns nothing.
TEST_F(AtMemoryQueryServiceTest,
       Query_NoLocalData_PersonalContextResolverEmpty_ReturnsEmpty) {
  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  fake_personal_context_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_TRUE(result.entries.empty());
  EXPECT_EQ(fake_personal_context_resolver->last_query(), u"what is my name");
}

// Tests that the query service correctly filters results when filter words
// are present in the query.
TEST_F(AtMemoryQueryServiceTest, Query_WithFilterWords) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult entry1(MemoryDataType::kAddressFull, u"Address",
                            u"123 San Diego St Home San Diego");
  MemorySearchResult entry2(MemoryDataType::kAddressFull, u"Address",
                            u"456 Mountain View Rd Work Mountain View");

  fake_data_provider->SetResults({entry1, entry2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in San Diego",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kAddressFull);
}

// Tests that the query service falls back to returning all results for the
// classified intent if none of the results match the filter words.
TEST_F(AtMemoryQueryServiceTest, Query_WithFilterWords_NoMatch_ReturnsAll) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult entry(MemoryDataType::kAddressFull, u"Address",
                           u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({entry});

  // "New York" won't match "San Diego", so it should fallback to returning all
  // results for that intent.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kAddressFull);
}

// Tests that the query service records the provider result count metric.
TEST_F(AtMemoryQueryServiceTest, RecordsProviderResultCountMetric) {
  base::HistogramTester histogram_tester;

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      /*delegate=*/nullptr, std::move(data_provider),
      /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  fake_data_provider->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectUniqueSample(
      "AccessibilityAnnotator.AtMemoryQueryService.ProviderResultCount."
      "AutofillDataProvider",
      /*sample=*/2, /*expected_bucket_count=*/1);
}

// Tests that the query service queries the personal context resolver if local
// filtering removes all results.
TEST_F(AtMemoryQueryServiceTest,
       Query_WithFilterWords_NoMatch_QueriesPersonalContextResolver) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(MemoryDataType::kAddressFull, u"Address",
                                 u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({local_entry});

  MemorySearchResult personal_context_entry(MemoryDataType::kAddressFull,
                                            u"Address",
                                            u"456 New York Ave Home New York");
  fake_personal_context_resolver->set_results({personal_context_entry});

  // "New York" won't match the local "San Diego" address, so it should
  // fallback to querying the personal context resolver.
  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"456 New York Ave Home New York")));
  EXPECT_EQ(fake_personal_context_resolver->last_query(),
            u"What's my home address in New York");
}

// Tests that the query service falls back to original local entries if the
// personal context resolver returns no results.
TEST_F(
    AtMemoryQueryServiceTest,
    Query_WithFilterWords_NoMatch_PersonalContextResolverEmpty_ReturnsLocal) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(MemoryDataType::kAddressFull, u"Address",
                                 u"123 San Diego St Home San Diego");
  fake_data_provider->SetResults({local_entry});

  // The personal context resolver returns nothing.
  fake_personal_context_resolver->set_results({});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"What's my home address in New York",
                 future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value,
                                  u"123 San Diego St Home San Diego")));
  EXPECT_EQ(fake_personal_context_resolver->last_query(),
            u"What's my home address in New York");
}

// Tests that the query service queries the personal context resolver and merges
// results if local data is found.
TEST_F(AtMemoryQueryServiceTest,
       Query_QueriesPersonalContextAndMergesIfLocalDataFound) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(MemoryDataType::kNameFull, u"Name",
                                 u"John Doe");
  fake_data_provider->SetResults({local_entry});

  MemorySearchResult personal_context_entry(MemoryDataType::kNameFull, u"Name",
                                            u"Jane Doe");
  fake_personal_context_resolver->set_results({personal_context_entry});

  base::test::RepeatingTestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetCallback());

  MemorySearchResults partial_result = future.Take();
  EXPECT_EQ(partial_result.status, MemorySearchStatus::kPartialResponseSuccess);
  EXPECT_THAT(partial_result.entries,
              testing::UnorderedElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe")));

  MemorySearchResults final_result = future.Take();
  EXPECT_EQ(final_result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(final_result.entries,
              testing::UnorderedElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe"),
                  testing::Field(&MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_personal_context_resolver->last_query(), u"what is my name");
}

// Tests that the query service returns the appropriate error status when the
// personal context resolver fails.
TEST_F(AtMemoryQueryServiceTest, Query_PersonalContextResolverError) {
  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::make_unique<FakeMemoryDataProvider>(),
      std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  fake_personal_context_resolver->set_error(
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kPermissionDenied));

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kInternalFailure);
  EXPECT_TRUE(result.entries.empty());
}

// Tests that the query service returns the local fallback entries even when
// the personal context resolver fails.
TEST_F(AtMemoryQueryServiceTest,
       Query_PersonalContextResolverError_ReturnsLocalFallback) {
  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();

  auto personal_context_resolver =
      std::make_unique<FakePersonalContextResolver>();
  auto* fake_personal_context_resolver = personal_context_resolver.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), std::move(personal_context_resolver),
      /*remote_model_executor=*/nullptr);

  MemorySearchResult local_entry(MemoryDataType::kNameFull, u"Name",
                                 u"John Doe");
  fake_data_provider->SetResults({local_entry});

  fake_personal_context_resolver->set_error(
      personal_context::ContextMemoryError::FromExecutionError(
          personal_context::ContextMemoryError::ExecutionError::
              kPermissionDenied));

  base::test::RepeatingTestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetCallback());

  // We should first get the partial success with local entries.
  MemorySearchResults partial_result = future.Take();
  EXPECT_EQ(partial_result.status, MemorySearchStatus::kPartialResponseSuccess);
  EXPECT_THAT(partial_result.entries,
              testing::UnorderedElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe")));

  // Then we should get the final result indicating the error, but containing
  // the fallback local entries.
  MemorySearchResults final_result = future.Take();
  EXPECT_EQ(final_result.status, MemorySearchStatus::kInternalFailure);
  EXPECT_THAT(final_result.entries,
              testing::UnorderedElementsAre(
                  testing::Field(&MemorySearchResult::value, u"John Doe")));
}

// Tests that the query service does not send results for a query that has been
// superseded by a newer query.
TEST_F(AtMemoryQueryServiceTest, StaleResultsAreNotSent) {
  auto data_provider = std::make_unique<DelayedMemoryDataProvider>();
  auto* fake_data_provider = data_provider.get();
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  base::test::TestFuture<MemorySearchResults> future1;
  service->Query(u"what is my name", future1.GetRepeatingCallback());

  // Start a second query before the first one completes.
  base::test::TestFuture<MemorySearchResults> future2;
  service->Query(u"what is my address", future2.GetRepeatingCallback());

  // Complete the first query's data retrieval.
  fake_data_provider->CompleteNext();

  // The first query's callback should NOT be called.
  EXPECT_FALSE(future1.IsReady());

  // Complete the second query's data retrieval.
  fake_data_provider->CompleteNext();

  // The second query's callback should be called.
  ASSERT_TRUE(future2.Wait());
}

// Tests that deduplication preserves the original insertion order.
TEST_F(AtMemoryQueryServiceTest, Query_DeduplicatesResults_PreservesOrder) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider1), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"Alice");
  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"Bob");
  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name",
                             u"Alice");  // duplicate of result1
  MemorySearchResult result4(MemoryDataType::kNameFull, u"Name", u"Charlie");

  fake_data_provider1->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const MemorySearchResults& result = future.Get();
  EXPECT_THAT(result.entries,
              testing::ElementsAre(
                  testing::Field(&MemorySearchResult::value, u"Alice"),
                  testing::Field(&MemorySearchResult::value, u"Bob"),
                  testing::Field(&MemorySearchResult::value, u"Charlie")));
}

// Tests that deduplication retains fields like confidence_score from the first
// entry and merges sources.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_RetainsFirstEntryFieldsAndMergesSources) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider1), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  EntryMetadata metadata(MemoryDataType::kAddressCity, u"City", u"San Diego");

  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.9);
  result1.metadata_list.push_back(metadata);
  result1.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe",
                             /*confidence_score=*/0.5);
  result2.metadata_list.push_back(metadata);
  result2.sources.push_back(MemoryEntrySource(MemoryEntrySourceType::kGmail));
  // Duplicate source shouldn't be added twice.
  result2.sources.push_back(
      MemoryEntrySource(MemoryEntrySourceType::kAutofill));

  fake_data_provider1->SetResults({result1, result2});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 1u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  EXPECT_DOUBLE_EQ(result.entries[0].confidence_score, 0.9);
  ASSERT_EQ(result.entries[0].sources.size(), 2u);
  EXPECT_EQ(result.entries[0].sources[0].type,
            MemoryEntrySourceType::kAutofill);
  EXPECT_EQ(result.entries[0].sources[1].type, MemoryEntrySourceType::kGmail);
}

// Tests that entries with different values or metadata lists are both retained.
TEST_F(AtMemoryQueryServiceTest,
       Query_DeduplicatesResults_KeepsDifferentEntries) {
  auto data_provider1 = std::make_unique<FakeMemoryDataProvider>();
  auto* fake_data_provider1 = data_provider1.get();

  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider1), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);

  EntryMetadata metadata_sd(MemoryDataType::kAddressCity, u"City",
                            u"San Diego");
  EntryMetadata metadata_ny(MemoryDataType::kAddressCity, u"City", u"New York");

  // Same value, different metadata
  MemorySearchResult result1(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result1.metadata_list.push_back(metadata_sd);

  MemorySearchResult result2(MemoryDataType::kNameFull, u"Name", u"John Doe");
  result2.metadata_list.push_back(metadata_ny);

  // Different value, same metadata
  MemorySearchResult result3(MemoryDataType::kNameFull, u"Name", u"Jane Doe");
  result3.metadata_list.push_back(metadata_sd);

  // Same value and metadata, different type
  MemorySearchResult result4(MemoryDataType::kUnknown, u"Unknown", u"John Doe");
  result4.metadata_list.push_back(metadata_sd);

  fake_data_provider1->SetResults({result1, result2, result3, result4});

  base::test::TestFuture<MemorySearchResults> future;
  service->Query(u"what is my name", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  ASSERT_EQ(result.entries.size(), 4u);
  EXPECT_EQ(result.entries[0].value, u"John Doe");
  ASSERT_EQ(result.entries[0].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[0].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[0].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[1].value, u"John Doe");
  ASSERT_EQ(result.entries[1].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[1].metadata_list[0].value, u"New York");
  EXPECT_EQ(result.entries[1].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[2].value, u"Jane Doe");
  ASSERT_EQ(result.entries[2].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[2].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[2].type, MemoryDataType::kNameFull);

  EXPECT_EQ(result.entries[3].value, u"John Doe");
  ASSERT_EQ(result.entries[3].metadata_list.size(), 1u);
  EXPECT_EQ(result.entries[3].metadata_list[0].value, u"San Diego");
  EXPECT_EQ(result.entries[3].type, MemoryDataType::kUnknown);
}

TEST_F(AtMemoryQueryServiceTest,
       Query_PersonalContextDebug_CustomTypeParam_ReturnsConfiguredType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      personal_context::features::debug::kMockPersonalContextResult,
      {{personal_context::features::debug::kMockPersonalContextResultTypeParam
            .name,
        "1"}});

  auto data_provider = std::make_unique<FakeMemoryDataProvider>();
  FakeMemoryDataProvider* fake_data_provider = data_provider.get();

  MemorySearchResult name_entry(MemoryDataType::kNameFull, u"Name",
                                u"Jane Doe");
  fake_data_provider->SetResults({name_entry});

  base::test::TestFuture<MemorySearchResults> future;
  auto service = std::make_unique<AtMemoryQueryService>(
      std::make_unique<MockAtMemoryQueryServiceDelegate>(),
      std::move(data_provider), /*personal_context_resolver=*/nullptr,
      /*remote_model_executor=*/nullptr);
  service->Query(u"random query", future.GetRepeatingCallback());

  ASSERT_TRUE(future.Wait());
  const auto& result = future.Get();
  EXPECT_EQ(result.status, MemorySearchStatus::kFinalResponseSuccess);
  EXPECT_THAT(result.entries, testing::ElementsAre(testing::Field(
                                  &MemorySearchResult::value, u"Jane Doe")));
  EXPECT_EQ(fake_data_provider->last_type(), MemoryDataType::kNameFull);
}

}  // namespace

}  // namespace accessibility_annotator
