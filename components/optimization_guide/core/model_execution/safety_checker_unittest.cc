// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/safety_checker.h"

#include <cstdint>
#include <initializer_list>
#include <sstream>
#include <string>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_file.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test.pb.h"
#include "base/test/test_future.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_descriptors.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/safety_client.h"
#include "components/optimization_guide/core/model_execution/safety_config.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "services/on_device_model/public/cpp/text_safety_assets.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::ResultOf;

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}
const google::protobuf::RepeatedField<float>& GetScores(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.response().text_safety_model_response().scores();
}
bool GetIsUnsafe(const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.response().text_safety_model_response().is_unsafe();
}

proto::ComposeRequest UrlAndInputRequest(const std::string& url,
                                         const std::string& input) {
  proto::ComposeRequest req;
  req.mutable_page_metadata()->set_page_url(url);
  req.mutable_generate_params()->set_user_input(input);
  return req;
}

proto::Any SimpleResponse(const std::string& output) {
  proto::ComposeResponse resp;
  resp.set_output(output);
  return AnyWrapProto(resp);
}

}  // namespace

class SafetyClientFixture {
 public:
  explicit SafetyClientFixture(proto::FeatureTextSafetyConfiguration config)
      : safety_asset_(std::move(config)) {
    safety_client_.SetLanguageDetectionModel(language_asset_.model_info());
    safety_client_.MaybeUpdateSafetyModel(safety_asset_.model_info());
  }

  std::unique_ptr<SafetyChecker> MakeSafetyChecker() {
    return safety_client_
        .MakeSafetyChecker(ModelBasedCapabilityKey::kCompose, false)
        .value();
  }

 private:
  FakeLanguageModelAsset language_asset_;
  FakeSafetyModelAsset safety_asset_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  on_device_model::ServiceClient service_client_{fake_launcher_.LaunchFn()};
  SafetyClient safety_client_{service_client_.GetWeakPtr()};
};

class SafetyCheckerTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  base::test::TestFuture<SafetyChecker::Result> future_;
};

TEST(SafetyConfigTest, MissingScoreIsUnsafe) {
  auto safety_config = ComposeSafetyConfig();
  auto* threshold = safety_config.add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1};  // Only 1 score, but expects 2.
  EXPECT_TRUE(cfg.IsUnsafeText(safety_info));
}

TEST(SafetyConfigTest, SafeWithRequiredScores) {
  auto safety_config = ComposeSafetyConfig();
  auto* threshold = safety_config.add_safety_category_thresholds();
  threshold->set_output_index(1);
  threshold->set_threshold(0.5);
  SafetyConfig cfg(safety_config);

  auto safety_info = on_device_model::mojom::SafetyInfo::New();
  safety_info->class_scores = {0.1, 0.1};  // Has score with index = 1.
  EXPECT_FALSE(cfg.IsUnsafeText(safety_info));
}

TEST_F(SafetyCheckerTest, RawOutputCheckPassesWithTrivialConfig) {
  // When no thresholds are defined, all outputs will pass.
  SafetyClientFixture fixture([]() { return ComposeSafetyConfig(); }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("unsafe raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(
                  ResultOf("check text", &GetCheckText, "unsafe raw output"),
                  ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                  ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, DefaultOutputSafetyPassesOnSafeOutput) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    // No explicit raw_output_check, should still perform a default one.
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("reasonable raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText, "reasonable raw output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, DefaultOutputSafetyFailsOnUnsafeOutput) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    // No explicit raw_output_check, should still perform a default one.
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("unsafe raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(
                  ResultOf("check text", &GetCheckText, "unsafe raw output"),
                  ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                  ResultOf("is_unsafe", &GetIsUnsafe, true))));
}

TEST_F(SafetyCheckerTest, OutputSafetyPassesWithMetRequiredLanguage) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("reasonable raw output in esperanto",
                             future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText,
                   "is_raw_output_safe: reasonable raw output in esperanto"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, OutputSafetyFailsWithUnmetRequiredLanguage) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("reasonable raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: reasonable raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest,
       OutputSafetyPassesWithSafeOutputAndNoRequiredLanguage) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("reasonable raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: reasonable raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, OutputSafetyFailsWithUnsafeOutput) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    auto* check = safety_config.mutable_raw_output_check();
    check->mutable_input_template()->Add(
        FieldSubstitution("is_raw_output_safe: %s", StringValueField()));
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRawOutputCheck("unsafe raw output", future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_raw_output_safe: unsafe raw output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, true))));
}

TEST_F(SafetyCheckerTest, RequestChecksPassesWithTrivialConfig) {
  SafetyClientFixture fixture([]() { return ComposeSafetyConfig(); }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UrlAndInputRequest("unsafe_url", "unsafe_input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs, ElementsAre());
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithSafeUrl) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_url: %s", PageUrlField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    {
      auto* check2 = safety_config.add_request_check();
      check2->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UrlAndInputRequest("safe_url", "reasonable input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText, "is_safe_url: safe_url"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, false)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "is_safe_input: reasonable input"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnsafeUrl) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_url: %s", PageUrlField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    {
      auto* check2 = safety_config.add_request_check();
      check2->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(
      UrlAndInputRequest("unsafe_url", "reasonable input"),
      future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_url: unsafe_url"),
                        ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, true)),
                  AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_input: reasonable input"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnmetRequiredLanguage) {
  SafetyClientFixture fixture([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UserInputRequest("english input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText, "is_safe_input: english input"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest,
       RequestCheckPassesWithUnmetRequiredLanguageButIgnored) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UserInputRequest("english input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(
          ResultOf("check text", &GetCheckText, "is_safe_input: english input"),
          ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
          ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithMetRequiredLanguage) {
  SafetyClientFixture fixture([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UserInputRequest("esperanto input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "is_safe_input: esperanto input"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckPassesWithLanguageOnly) {
  SafetyClientFixture fixture([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      // Note: these thresholds are ignored.
      check->mutable_safety_category_thresholds()->Add(RequireReasonable());
      check->set_check_language_only(true);
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UserInputRequest("esperanto input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                         "is_safe_input: esperanto input"),
                                ResultOf("scores", &GetScores, IsEmpty()),
                                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithLanguageOnly) {
  SafetyClientFixture fixture([]() {
    // Configure a request safety check on the PageUrl.
    auto safety_config = ComposeSafetyConfig();
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("is_safe_input: %s", UserInputField()));
      // Note: these thresholds are ignored.
      check->mutable_safety_category_thresholds()->Add(RequireReasonable());
      check->set_check_language_only(true);
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunRequestChecks(UserInputRequest("english input"),
                            future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(result.logs,
              ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                         "is_safe_input: english input"),
                                ResultOf("scores", &GetScores, IsEmpty()),
                                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, ResponseCheckPassesWithSafeResponse) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunResponseChecks(
      UrlAndInputRequest("very_", "reasonable_esperanto_"),
      SimpleResponse("safe_output"), future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check: very_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, false)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check2: reasonable_esperanto_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, RequestCheckFailsWithUnsafeResponse) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunResponseChecks(UrlAndInputRequest("un", "reasonable_esperanto_"),
                             SimpleResponse("safe_output"),
                             future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_TRUE(result.is_unsafe);
  EXPECT_FALSE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check: unsafe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.8, 0.8)),
                ResultOf("is_unsafe", &GetIsUnsafe, true)),
          AllOf(ResultOf("check text", &GetCheckText,
                         "response_check2: reasonable_esperanto_safe_output"),
                ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

TEST_F(SafetyCheckerTest, ResponseCheckFailsWithUnmetRequiredLanguge) {
  SafetyClientFixture fixture([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.add_allowed_languages("eo");  // Require esperanto
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_REQUEST);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check2: %s", UserInputField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
    }
    return safety_config;
  }());
  auto checker = fixture.MakeSafetyChecker();
  checker->RunResponseChecks(UrlAndInputRequest("very_", "reasonable_"),
                             SimpleResponse("safe_output"),
                             future_.GetCallback());
  auto result = future_.Take();

  EXPECT_FALSE(result.failed_to_run);
  EXPECT_FALSE(result.is_unsafe);
  EXPECT_TRUE(result.is_unsupported_language);
  EXPECT_THAT(
      result.logs,
      ElementsAre(AllOf(ResultOf("check text", &GetCheckText,
                                 "response_check: very_safe_output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.8)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false)),
                  AllOf(ResultOf("check text", &GetCheckText,
                                 "response_check2: reasonable_safe_output"),
                        ResultOf("scores", &GetScores, ElementsAre(0.2, 0.2)),
                        ResultOf("is_unsafe", &GetIsUnsafe, false))));
}

}  // namespace optimization_guide
