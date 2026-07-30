// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/suggestion/filter_suggestion_generator.h"

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/annotation_index/mock_annotation_index_client.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "components/multistep_filter/core/data_models/filter_suggestion_candidate.h"
#include "components/multistep_filter/core/data_models/url_filter_suggestion.h"
#include "components/multistep_filter/core/features.h"
#include "components/multistep_filter/core/storage/filter_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using internal::kDefaultMaxResults;
constexpr char kTestUrl[] = "https://example.com";
constexpr char kTestDomain[] = "example.com";
constexpr char kShoppingTask[] = "SHOPPING";
constexpr char kTestAttributeKey[] = "category";
constexpr char kTestAttributeValue[] = "shoes";
constexpr char16_t kTestAttributeValue16[] = u"shoes";
constexpr char kTestAttributeKey2[] = "size";
constexpr char kTestAttributeValue2[] = "large";
constexpr char16_t kTestAttributeValue2_16[] = u"large";
constexpr char kTestAttributeKey3[] = "color";
constexpr char kTestAttributeValue3[] = "red";
constexpr char16_t kTestAttributeValue3_16[] = u"red";
constexpr char kTestSuggestionUrl[] = "https://example.com/shoes";
constexpr int64_t kTestNavigationId = 12345;

FilterAnnotation CreateDummyAnnotation(
    std::string task_type,
    std::string source_domain,
    std::vector<FilterAttribute> attributes) {
  std::string host = "sub." + source_domain;
  return FilterAnnotation(base::Uuid::GenerateRandomV4(), std::move(task_type),
                          std::move(source_domain), std::move(host),
                          base::Time::Now(), std::move(attributes));
}

using testing::_;
using testing::Return;

class MockFilterStore : public FilterStore {
 public:
  MockFilterStore() = default;
  ~MockFilterStore() override = default;

  MOCK_METHOD(void,
              GetAnnotationsForTaskSortedByCreationTimestamp,
              (std::string task_type,
               base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
               size_t max_count,
               base::Time min_creation_time),
              (override));
};

class FilterSuggestionGeneratorTest : public testing::Test {
 public:
  FilterSuggestionGeneratorTest() = default;
  ~FilterSuggestionGeneratorTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        kMultistepFilter,
        {{"CueTemplatesMap", "{\"SHOPPING\": {\"template\": \"Template\"}}"},
         {"SameDomainSuggestionSuppressionDuration", "0s"}});
    store_ = std::make_unique<testing::NiceMock<MockFilterStore>>();
    generator_ = std::make_unique<FilterSuggestionGenerator>(
        mock_client_, *store_, /*log_router=*/nullptr);
  }

  void TearDown() override {
    generator_.reset();
    if (store_) {
      store_.reset();
      base::ThreadPoolInstance::Get()->FlushForTesting();
    }
  }

 protected:
  MockAnnotationIndexClient& mock_client() { return mock_client_; }
  MockFilterStore* store() { return store_.get(); }
  FilterSuggestionGenerator* generator() { return generator_.get(); }
  void DestroyGenerator() { generator_.reset(); }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  const std::vector<std::string> kSupportedTaskTypes = {kShoppingTask};

 private:
  base::test::ScopedFeatureList feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  testing::NiceMock<MockAnnotationIndexClient> mock_client_;
  std::unique_ptr<testing::NiceMock<MockFilterStore>> store_;
  std::unique_ptr<FilterSuggestionGenerator> generator_;
};

// Tests that a valid suggestion is successfully generated when both index
// client and filter store return valid data.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuccessfulSuggestionGenerated) {
  const GURL url(kTestUrl);

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  FilterSuggestionCandidate expected_candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  std::vector<FilterAttributeUiLabel> attribute_ui_labels;
  attribute_ui_labels.emplace_back(expected_candidate.attributes[0],
                                   attributes[0]);
  attribute_ui_labels.emplace_back(expected_candidate.attributes[1],
                                   attributes[1]);
  UrlFilterSuggestion expected_suggestion(UrlFilterSuggestion::Params{
      .navigation_url = expected_candidate.navigation_url,
      .source_domain = base::UTF8ToUTF16(annotation.source_domain),
      .source_host = base::UTF8ToUTF16(annotation.source_host),
      .extraction_timestamp = annotation.creation_timestamp,
      .attribute_ui_labels = std::move(attribute_ui_labels),
      .triggering_navigation_id = kTestNavigationId,
      .triggering_domain = kTestDomain,
      .triggering_host = kTestDomain,
      .task_type = kShoppingTask,
      .suggestion_message = u"Template"});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([expected_candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        ASSERT_EQ(filter_annotations.size(), 1u);
        EXPECT_EQ(filter_annotations[0].task_type, kShoppingTask);
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{expected_candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), expected_suggestion);
}

// Tests that suggestion generation is suppressed if the candidate URL is
// identical to the current triggering URL.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesSubsumedSuggestions) {
  const GURL url("https://example.com/search?category=shoes&size=large");

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Identical URL should be suppressed.
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that suggestion generation is suppressed if the candidate URL's query
// parameters are a strict subset of the current URL's parameters.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesSubsetParameters) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Candidate URL is a subset of parameters of the triggering URL.
  FilterSuggestionCandidate candidate(
      annotation.id, GURL("https://example.com/search?category=shoes"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that a candidate URL with a different base path is not suppressed even
// if its query parameters match.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_DoesNotSuppressDifferentBaseUrl) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2},
      {kTestAttributeKey3, kTestAttributeValue3}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Different base URL should NOT be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/other?category=shoes&size=large"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_TRUE(future.Get().has_value());
}

// Tests that a candidate URL with additional query parameters beyond the
// current URL's parameters is not suppressed.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_DoesNotSuppressAdditionalParameters) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2},
      {kTestAttributeKey3, kTestAttributeValue3}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Additional parameters should NOT be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large&sort=new"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_TRUE(future.Get().has_value());
}

// Tests that suggestion generation is suppressed if the candidate has only one
// matching filter attribute.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesOneAttribute) {
  const GURL url("https://example.com/search?category=shoes&size=large");
  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Candidate has exactly 1 attribute! So it should be suppressed!
  FilterSuggestionCandidate candidate(
      annotation.id,
      GURL("https://example.com/search?category=shoes&size=large&sort=new"),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16)});
  EXPECT_CALL(mock_client(), GetFilterSuggestionCandidates)
      .WillOnce([candidate](
                    const GURL&, base::span<const FilterAnnotation>,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that only candidate attributes with matching keys in the annotation are
// included in the suggestion.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_OnlyMatchesPresentKeys) {
  const GURL url(kTestUrl);

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain,
                            {{kTestAttributeKey, kTestAttributeValue},
                             {kTestAttributeKey3, kTestAttributeValue3}});

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Candidate has key2 (missing in annotation) and key1 (present).
  FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey3,
                                          kTestAttributeValue3_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  std::optional<UrlFilterSuggestion> result = future.Get();
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->attribute_ui_labels.size(), 2u);
  EXPECT_EQ(result->attribute_ui_labels[0].attribute_label,
            kTestAttributeValue16);
  EXPECT_EQ(result->attribute_ui_labels[0].attribute_value,
            kTestAttributeValue16);
  EXPECT_EQ(result->attribute_ui_labels[1].attribute_label,
            kTestAttributeValue3_16);
  EXPECT_EQ(result->attribute_ui_labels[1].attribute_value,
            kTestAttributeValue3_16);
}

// Tests that std::nullopt is returned when no attribute keys match between the
// candidate and the annotation.
TEST_F(FilterSuggestionGeneratorTest, GenerateSuggestion_NoMatchingKeys) {
  const GURL url(kTestUrl);

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, {{"key1", "val1"}});

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute("key2", u"label2")});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that std::nullopt is returned when the server returns an empty list of
// supported task types.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_EmptySupportedTaskTypesReturnsNullopt) {
  const GURL url(kTestUrl);

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, std::vector<std::string>(),
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that std::nullopt is returned when no historical annotations are found
// in the filter store.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_NoAnnotationsReturnsNullopt) {
  const GURL url(kTestUrl);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [](std::string task_type,
             base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
             size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>());
          });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that std::nullopt is returned when the candidate's annotation ID does
// not match any retrieved annotation.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CandidateWithNoMatchingAnnotationReturnsNullopt) {
  const GURL url(kTestUrl);

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  // Create a candidate with a non-matching annotation ID.
  FilterSuggestionCandidate candidate(
      base::Uuid::GenerateRandomV4(), GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that the callback is invoked with std::nullopt if the annotation index
// client drops the callback.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CallbackInvokedWhenClientDropsIt) {
  const GURL url(kTestUrl);

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, {{"key1", "val1"}});
  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          std::vector<FilterAnnotation>{annotation}));

  base::OnceCallback<void(
      std::optional<std::vector<FilterSuggestionCandidate>>)>
      captured_cb;
  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce(
          [&](const GURL& u,
              base::span<const FilterAnnotation> filter_annotations,
              base::OnceCallback<void(
                  std::optional<std::vector<FilterSuggestionCandidate>>)>
                  callback,
              int64_t navigation_id) { captured_cb = std::move(callback); });
  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;

  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  ASSERT_FALSE(future.IsReady());

  captured_cb.Reset();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that the callback is invoked with std::nullopt if the generator is
// destroyed while a request is pending.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_CallbackInvokedWhenGeneratorDestroyed) {
  const GURL url(kTestUrl);

  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, {{"key1", "val1"}});
  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(base::test::RunOnceCallback<1>(
          std::vector<FilterAnnotation>{annotation}));

  base::OnceCallback<void(
      std::optional<std::vector<FilterSuggestionCandidate>>)>
      captured_cb;
  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce(
          [&](const GURL& u,
              base::span<const FilterAnnotation> filter_annotations,
              base::OnceCallback<void(
                  std::optional<std::vector<FilterSuggestionCandidate>>)>
                  callback,
              int64_t navigation_id) { captured_cb = std::move(callback); });
  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;

  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  ASSERT_FALSE(future.IsReady());

  // The callback is bound to a `WeakPtr` of the generator, so the `WeakPtr` is
  // now invalidated.
  DestroyGenerator();
  captured_cb.Reset();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that std::nullopt is returned if cue message generation fails for the
// matching task type.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_SuppressesWhenMessageFails) {
  const GURL url(kTestUrl);

  std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation("NON_SHOPPING", kTestDomain, attributes);

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            "NON_SHOPPING", _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});

  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future;
  generator()->GenerateSuggestion(url, std::vector<std::string>{"NON_SHOPPING"},
                                  future.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  // Should be suppressed (returns nullopt) because message generation failed!
  EXPECT_EQ(future.Get(), std::nullopt);
}

// Tests that suggestion generation is throttled if an annotation was recently
// extracted from the same host.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_ThrottlesRecentExtractions) {
  const GURL url(kTestUrl);
  const base::Time now = base::Time::Now();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter,
      {{"CueTemplatesMap", "{\"SHOPPING\": {\"template\": \"Template\"}}"},
       {"SameDomainSuggestionSuppressionDuration", "5m"}});

  const std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, kTestDomain, attributes);
  annotation.creation_timestamp = now;

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillRepeatedly(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  task_environment().AdvanceClock(base::Minutes(2));

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future1;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future1.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_EQ(future1.Get(), std::nullopt);

  task_environment().AdvanceClock(base::Minutes(4));

  const FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future2;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future2.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_TRUE(future2.Get().has_value());
}

// Tests that recent-extraction throttling does not apply if the recent
// annotation is from a different domain.
TEST_F(FilterSuggestionGeneratorTest,
       GenerateSuggestion_DoesNotThrottleDifferentDomain) {
  const GURL url(kTestUrl);
  const base::Time now = base::Time::Now();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      kMultistepFilter,
      {{"CueTemplatesMap", "{\"SHOPPING\": {\"template\": \"Template\"}}"},
       {"SameDomainSuggestionSuppressionDuration", "5m"}});

  const std::vector<FilterAttribute> attributes = {
      {kTestAttributeKey, kTestAttributeValue},
      {kTestAttributeKey2, kTestAttributeValue2}};
  FilterAnnotation annotation =
      CreateDummyAnnotation(kShoppingTask, "different-domain.com", attributes);
  annotation.creation_timestamp = now;

  EXPECT_CALL(*store(), GetAnnotationsForTaskSortedByCreationTimestamp(
                            kShoppingTask, _, kDefaultMaxResults, _))
      .WillOnce(
          [annotation](
              std::string task_type,
              base::OnceCallback<void(std::vector<FilterAnnotation>)> callback,
              size_t max_count, base::Time min_creation_time) {
            std::move(callback).Run(std::vector<FilterAnnotation>{annotation});
          });

  task_environment().AdvanceClock(base::Minutes(2));

  const FilterSuggestionCandidate candidate(
      annotation.id, GURL(kTestSuggestionUrl),
      {FilterSuggestionCandidateAttribute(kTestAttributeKey,
                                          kTestAttributeValue16),
       FilterSuggestionCandidateAttribute(kTestAttributeKey2,
                                          kTestAttributeValue2_16)});
  EXPECT_CALL(mock_client(),
              GetFilterSuggestionCandidates(url, _, _, kTestNavigationId))
      .WillOnce([candidate](
                    const GURL& u,
                    base::span<const FilterAnnotation> filter_annotations,
                    base::OnceCallback<void(
                        std::optional<std::vector<FilterSuggestionCandidate>>)>
                        callback,
                    int64_t navigation_id) {
        std::move(callback).Run(
            std::vector<FilterSuggestionCandidate>{candidate});
      });

  base::test::TestFuture<std::optional<UrlFilterSuggestion>> future1;
  generator()->GenerateSuggestion(url, kSupportedTaskTypes,
                                  future1.GetCallback(), kTestNavigationId,
                                  kTestDomain);

  EXPECT_TRUE(future1.Get().has_value());
}

}  // namespace

}  // namespace multistep_filter
