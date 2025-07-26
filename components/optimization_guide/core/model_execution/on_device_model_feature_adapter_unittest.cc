// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"

#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/parser_kind.pb.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

using ParseResponseFuture =
    base::test::TestFuture<base::expected<proto::Any, ResponseParsingError>>;

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestNoSafetyFallbackConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(
                              MultimodalMessageReadView(request), "text"));
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestNoUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.mutable_text_safety_fallback_config();
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_generate_params()->set_user_input("whatever");
  auto safety_request = adapter->ConstructTextSafetyRequest(
      MultimodalMessageReadView(request), "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_TRUE(safety_request->url().empty());
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructTextSafetyRequestWithUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(3);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(1);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  auto safety_request = adapter->ConstructTextSafetyRequest(
      MultimodalMessageReadView(request), "text");

  ASSERT_TRUE(safety_request);
  EXPECT_EQ("text", safety_request->text());
  EXPECT_EQ("url", safety_request->url());
}

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructTextSafetyRequestWithBadUrlField) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* ts_config = config.mutable_text_safety_fallback_config();
  auto* input_url_proto_field = ts_config->mutable_input_url_proto_field();
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  input_url_proto_field->add_proto_descriptors()->set_tag_number(100);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  proto::ComposeRequest request;
  request.mutable_page_metadata()->set_page_url("url");
  EXPECT_EQ(std::nullopt, adapter->ConstructTextSafetyRequest(
                              MultimodalMessageReadView(request), "text"));
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_NoInputConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_MismatchRequest) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("wrong name");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  EXPECT_FALSE(result);
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_ForInputContext) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* ctx_substitution = input_config->add_input_context_substitutions();
  ctx_substitution->set_string_template("hello this is %s");
  ctx_substitution->add_substitutions()->add_candidates()->set_raw_string(
      "input context");
  auto* execute_substitution = input_config->add_execute_substitutions();
  execute_substitution->set_string_template("this should be ignored");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/true);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is input context");
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructInputString_ForExecution) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* input_config = config.mutable_input_config();
  input_config->set_request_base_name("base.test.TestMessage");
  auto* ctx_substitution = input_config->add_input_context_substitutions();
  ctx_substitution->set_string_template("this should be ignored");
  auto* execute_substitution = input_config->add_execute_substitutions();
  execute_substitution->set_string_template("hello this is %s");
  execute_substitution->add_substitutions()->add_candidates()->set_raw_string(
      "execution");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  base::test::TestMessage request;
  auto result =
      adapter->ConstructInputString(MultimodalMessageReadView(request),
                                    /*want_input_context=*/false);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->ToString(), "hello this is execution");
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructOutputMetadata_NoOutputConfig) {
  proto::OnDeviceModelExecutionFeatureConfig config;

  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output", 0u, response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  EXPECT_FALSE(maybe_metadata.has_value());
  EXPECT_EQ(maybe_metadata.error(), ResponseParsingError::kFailed);
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructOutputMetadata_DefaultSimple) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* oc = config.mutable_output_config();
  oc->set_proto_type("optimization_guide.proto.ComposeResponse");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output", 0u, response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructOutputMetadata_DefaultSimple_ChunkByChunkWithStartingPos) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* oc = config.mutable_output_config();
  oc->set_proto_type("optimization_guide.proto.ComposeResponse");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  oc->set_response_streaming_mode(
      proto::ResponseStreamingMode::STREAMING_MODE_CHUNK_BY_CHUNK);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  // For `STREAMING_MODE_CHUNK_BY_CHUNK`, the response will start after the
  // `previous_response_pos`.
  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output",
                         /*previous_response_pos=*/3u,
                         response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "put",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(OnDeviceModelFeatureAdapterTest,
     ConstructOutputMetadata_DefaultSimple_CurrentResponseWithStartingPos) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* oc = config.mutable_output_config();
  oc->set_proto_type("optimization_guide.proto.ComposeResponse");
  oc->mutable_proto_field()->add_proto_descriptors()->set_tag_number(1);
  oc->set_response_streaming_mode(
      proto::ResponseStreamingMode::STREAMING_MODE_CURRENT_RESPONSE);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  // For `STREAMING_MODE_CURRENT_RESPONSE`, even if the `previous_response_pos`
  // is set, it will parse from the beginning.
  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "output",
                         /*previous_response_pos=*/3u,
                         response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "output",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(OnDeviceModelFeatureAdapterTest, ConstructOutputMetadata_JSON) {
  base::test::TaskEnvironment task_environment;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  proto::OnDeviceModelExecutionFeatureConfig config;
  auto* oc = config.mutable_output_config();
  oc->set_parser_kind(proto::PARSER_KIND_JSON);
  oc->set_proto_type("optimization_guide.proto.ComposeResponse");
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  ParseResponseFuture response_future;
  MultimodalMessage request((base::test::TestMessage()));
  adapter->ParseResponse(request, "{\"output\": \"abc\"}", 0u,
                         response_future.GetCallback());
  auto maybe_metadata = response_future.Get();

  ASSERT_TRUE(maybe_metadata.has_value());
  EXPECT_EQ(
      "abc",
      ParsedAnyMetadata<proto::ComposeResponse>(*maybe_metadata)->output());
}

TEST(OnDeviceModelFeatureAdapterTest, ShouldParseResponseCompleteOnly) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.mutable_output_config()->set_parser_kind(proto::PARSER_KIND_SIMPLE);
  config.mutable_output_config()->set_suppress_parsing_incomplete_output(true);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  EXPECT_FALSE(adapter->ShouldParseResponse(ResponseCompleteness::kPartial));
  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kComplete));
}

TEST(OnDeviceModelFeatureAdapterTest, ShouldParseResponseAlways) {
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.mutable_output_config()->set_parser_kind(proto::PARSER_KIND_SIMPLE);
  config.mutable_output_config()->set_suppress_parsing_incomplete_output(false);
  auto adapter =
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(std::move(config));

  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kPartial));
  EXPECT_TRUE(adapter->ShouldParseResponse(ResponseCompleteness::kComplete));
}

}  // namespace optimization_guide
