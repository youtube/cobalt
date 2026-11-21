// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/personal_context_resolver_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_data_type.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/personal_context/core/mock_personal_context_service.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/personal_context/proto/features/common_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::WithArg;

class PersonalContextResolverImplTest : public ::testing::Test {
 public:
  PersonalContextResolverImplTest() = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<personal_context::MockPersonalContextService> mock_service_;
};

// Tests that the resolver handles a null `PersonalContextService` gracefully by
// immediately posting an empty results callback.
TEST_F(PersonalContextResolverImplTest, QueryWhenNullServiceReturnsEmpty) {
  PersonalContextResolverImpl resolver(nullptr, "en-US");

  base::test::TestFuture<base::expected<std::vector<MemorySearchResult>,
                                        personal_context::ContextMemoryError>>
      future;
  resolver.Query(u"my query", future.GetCallback());
  auto result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(
      result.error().error(),
      personal_context::ContextMemoryError::ExecutionError::kGenericFailure);
}

// Tests that a Query request is correctly forwarded to the
// `PersonalContextService` with appropriate metadata and timeout.
TEST_F(PersonalContextResolverImplTest, QuerySendsCorrectRequest) {
  PersonalContextResolverImpl resolver(&mock_service_, "en-US");
  std::u16string query = u"my test query";

  EXPECT_CALL(
      mock_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, _,
                   _, _))
      .WillOnce([](personal_context::proto::ContextMemoryFeature feature,
                   const google::protobuf::MessageLite& request_metadata,
                   const personal_context::ContextMemoryRequestOptions&
                       options,
                   personal_context::FetchContextCallback callback) {
        const auto& request =
            static_cast<const personal_context::proto::AtMemoryQueryRequest&>(
                request_metadata);
        EXPECT_EQ(request.input_query(), "my test query");
        EXPECT_EQ(request.locale(), "en-US");
        EXPECT_GT(request.supported_local_data_types_size(), 0);
        EXPECT_EQ(request.supported_local_data_types(0),
                  personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
        EXPECT_EQ(options.request_timeout, base::Seconds(30));
      });

  resolver.Query(query, base::DoNothing());
}

// Tests that making a new query while another is in-flight will cancel the
// previous request and run its callback with an empty result set.
TEST_F(PersonalContextResolverImplTest, QuerySavesCallbackAndCancelsPrevious) {
  PersonalContextResolverImpl resolver(&mock_service_, "en-US");

  base::test::TestFuture<base::expected<std::vector<MemorySearchResult>,
                                        personal_context::ContextMemoryError>>
      future1;
  base::test::TestFuture<base::expected<std::vector<MemorySearchResult>,
                                        personal_context::ContextMemoryError>>
      future2;

  resolver.Query(u"query 1", future1.GetCallback());
  // The second query should immediately cancel the first and return cancelled
  // error.
  resolver.Query(u"query 2", future2.GetCallback());

  auto result1 = future1.Get();
  EXPECT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error().error(),
            personal_context::ContextMemoryError::ExecutionError::kCancelled);
}

// Tests that the resolver correctly translates a successful
// `AtMemoryQueryResponse` message into local `MemorySearchResult` structs,
// mapping fields (attributes, sources, and metadata) accordingly.
TEST_F(PersonalContextResolverImplTest,
       OnPersonalContextRetrievedParsesResponse) {
  PersonalContextResolverImpl resolver(&mock_service_, "en-US");

  personal_context::FetchContextCallback captured_callback;
  EXPECT_CALL(mock_service_, FetchContext(_, _, _, _))
      .WillOnce(
          WithArg<3>([&captured_callback](
                         personal_context::FetchContextCallback callback) {
            captured_callback = std::move(callback);
          }));

  base::test::TestFuture<base::expected<std::vector<MemorySearchResult>,
                                        personal_context::ContextMemoryError>>
      future;
  resolver.Query(u"some query", future.GetCallback());
  ASSERT_TRUE(captured_callback);

  // Prepare a fake server response.
  personal_context::proto::AtMemoryQueryResponse response;
  response.set_query_classification(
      personal_context::proto::AtMemoryQueryResponse::
          QUERY_CLASSIFICATION_AT_MEMORY);
  personal_context::proto::AtMemorySearchResult* result =
      response.add_results();
  result->set_relevance_score(1.0);

  // Primary Attribute: Schemaful key
  personal_context::proto::Attribute* primary =
      result->mutable_primary_attribute();
  primary->set_schemaful_key(
      personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
  primary->set_value("John Doe");

  personal_context::proto::SourceReference* source1 = result->add_sources();
  source1->mutable_gmail()->set_message_url("http://gmail.com/msg/1");
  personal_context::proto::SourceReference* source2 = result->add_sources();
  source2->mutable_photos()->set_photos_url("http://photos.google.com/item/2");

  // Secondary attribute
  personal_context::proto::Attribute* secondary =
      result->add_secondary_attributes();
  secondary->set_schemaful_key(personal_context::proto::MEMORY_DATA_TYPE_EMAIL);
  secondary->set_value("johndoe@example.com");

  // Pack the response into AtMemoryQueryResponse serialized string
  personal_context::proto::Any serialized_response;
  serialized_response.set_value(response.SerializeAsString());

  base::expected<const personal_context::proto::Any,
                 personal_context::ContextMemoryError>
      expected_response(serialized_response);
  personal_context::FetchContextResult fetch_result(
      std::move(expected_response));

  std::move(captured_callback).Run(std::move(fetch_result));

  auto query_result = future.Get();
  ASSERT_TRUE(query_result.has_value());
  std::vector<MemorySearchResult> parsed_results = query_result.value();
  ASSERT_EQ(parsed_results.size(), 1u);
  EXPECT_EQ(parsed_results[0].type, MemoryDataType::kNameFull);
  EXPECT_EQ(parsed_results[0].value, u"John Doe");
  EXPECT_DOUBLE_EQ(parsed_results[0].confidence_score, 1.0);

  ASSERT_EQ(parsed_results[0].sources.size(), 2u);
  EXPECT_EQ(parsed_results[0].sources[0].type, MemoryEntrySourceType::kGmail);
  EXPECT_EQ(parsed_results[0].sources[0].deeplink_url,
            "http://gmail.com/msg/1");
  EXPECT_EQ(parsed_results[0].sources[1].type, MemoryEntrySourceType::kPhotos);
  EXPECT_EQ(parsed_results[0].sources[1].deeplink_url,
            "http://photos.google.com/item/2");

  ASSERT_EQ(parsed_results[0].metadata_list.size(), 1u);
  EXPECT_EQ(parsed_results[0].metadata_list[0].type, MemoryDataType::kEmail);
  EXPECT_EQ(parsed_results[0].metadata_list[0].value, u"johndoe@example.com");
}

}  // namespace
}  // namespace accessibility_annotator
