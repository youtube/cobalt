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
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
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

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver.Query(u"my query", future.GetCallback());
  EXPECT_TRUE(future.Get().empty());
}

// Tests that a Query request is correctly forwarded to the
// `PersonalContextService` with appropriate metadata.
TEST_F(PersonalContextResolverImplTest, QuerySendsCorrectRequest) {
  PersonalContextResolverImpl resolver(&mock_service_, "en-US");
  std::u16string query = u"my test query";

  EXPECT_CALL(
      mock_service_,
      FetchContext(personal_context::proto::CONTEXT_MEMORY_FEATURE_AT_MEMORY, _,
                   _, _))
      .WillOnce(WithArg<1>([](const google::protobuf::MessageLite&
                                  request_metadata) {
        const auto& request =
            static_cast<const personal_context::proto::AtMemoryQueryRequest&>(
                request_metadata);
        EXPECT_EQ(request.input_query(), "my test query");
        EXPECT_EQ(request.locale(), "en-US");
        EXPECT_GT(request.supported_local_data_types_size(), 0);
        EXPECT_EQ(request.supported_local_data_types(0),
                  personal_context::proto::MEMORY_DATA_TYPE_NAME_FULL);
      }));

  resolver.Query(query, base::DoNothing());
}

// Tests that making a new query while another is in-flight will cancel the
// previous request and run its callback with an empty result set.
TEST_F(PersonalContextResolverImplTest, QuerySavesCallbackAndCancelsPrevious) {
  PersonalContextResolverImpl resolver(&mock_service_, "en-US");

  base::test::TestFuture<std::vector<MemorySearchResult>> future1;
  base::test::TestFuture<std::vector<MemorySearchResult>> future2;

  resolver.Query(u"query 1", future1.GetCallback());
  // The second query should immediately cancel the first and return empty.
  resolver.Query(u"query 2", future2.GetCallback());

  EXPECT_TRUE(future1.Get().empty());
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

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver.Query(u"some query", future.GetCallback());
  ASSERT_TRUE(captured_callback);

  // Prepare a fake server response.
  personal_context::proto::AtMemoryQueryResponse response;
  response.set_query_classification(
      personal_context::proto::AtMemoryQueryResponse::
          QUERY_CLASSIFICATION_AT_MEMORY);
  personal_context::proto::AtMemorySearchResult* result =
      response.add_results();

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

  std::vector<MemorySearchResult> parsed_results = future.Get();
  ASSERT_EQ(parsed_results.size(), 1u);
  EXPECT_EQ(parsed_results[0].type, EntryType::kNameFull);
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
  EXPECT_EQ(parsed_results[0].metadata_list[0].type, EntryType::kEmail);
  EXPECT_EQ(parsed_results[0].metadata_list[0].value, u"johndoe@example.com");
}

}  // namespace
}  // namespace accessibility_annotator
