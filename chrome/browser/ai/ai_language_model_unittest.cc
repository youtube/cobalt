// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_language_model.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/current_thread.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ai/ai_test_utils.h"
#include "chrome/browser/ai/ai_utils.h"
#include "chrome/browser/ai/features.h"
#include "chrome/browser/component_updater/optimization_guide_on_device_model_installer.h"
#include "components/optimization_guide/core/mock_optimization_guide_model_executor.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/descriptors.pb.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/string_value.pb.h"
#include "components/update_client/update_client.h"
#include "content/public/browser/render_widget_host_view.h"
#include "services/on_device_model/public/cpp/capabilities.h"
#include "services/on_device_model/public/mojom/on_device_model.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/ai/ai_common.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_language_model.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-shared.h"
#include "third_party/blink/public/mojom/ai/model_download_progress_observer.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-shared.h"

namespace {

using ::optimization_guide::FieldSubstitution;
using ::optimization_guide::ForbidUnsafe;
using ::optimization_guide::StringValueField;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Return;
using Role = ::blink::mojom::AILanguageModelPromptRole;

constexpr uint32_t kTestMaxContextToken = 10u;
constexpr uint32_t kTestDefaultTopK = 1u;
constexpr float kTestDefaultTemperature = 0.0f;
constexpr uint32_t kTestMaxTopK = 5u;
constexpr float kTestMaxTemperature = 1.5;
constexpr uint32_t kTestMaxTokens = 100u;
constexpr uint64_t kTestModelDownloadSize = 572u;
static_assert(kTestDefaultTopK <= kTestMaxTopK);
static_assert(kTestDefaultTemperature <= kTestMaxTemperature);

SkBitmap CreateTestBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorRED);
  return bitmap;
}

on_device_model::mojom::AudioDataPtr CreateTestAudio() {
  return on_device_model::mojom::AudioData::New();
}

// Convert a single AILanguageModelPromptContentPtr to a vector.
std::vector<blink::mojom::AILanguageModelPromptContentPtr> ToVector(
    blink::mojom::AILanguageModelPromptContentPtr content) {
  std::vector<blink::mojom::AILanguageModelPromptContentPtr> vector;
  vector.push_back(std::move(content));
  return vector;
}

optimization_guide::proto::FeatureTextSafetyConfiguration CreateSafetyConfig() {
  optimization_guide::proto::FeatureTextSafetyConfiguration safety_config;
  safety_config.set_feature(
      optimization_guide::proto::MODEL_EXECUTION_FEATURE_PROMPT_API);
  safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
  return safety_config;
}

// Build a mojo prompt struct with the specified `role` and `text`
blink::mojom::AILanguageModelPromptPtr MakePrompt(Role role,
                                                  const std::string& text) {
  return blink::mojom::AILanguageModelPrompt::New(
      role,
      ToVector(blink::mojom::AILanguageModelPromptContent::NewText(text)));
}

// Build a mojo prompt struct array holding a single piece of text.
std::vector<blink::mojom::AILanguageModelPromptPtr> MakeInput(
    const std::string& text) {
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts;
  prompts.push_back(MakePrompt(Role::kUser, text));
  return prompts;
}

// Construct a ContextItem with system prompt text.
AILanguageModel::Context::ContextItem SimpleContextItem(std::string text,
                                                        uint32_t size) {
  auto item = AILanguageModel::Context::ContextItem();
  item.tokens = size;
  item.input = on_device_model::mojom::Input::New();
  item.input->pieces = {ml::Token::kSystem, text};
  return item;
}

// Convert a ml::Token to a string for expectation matching.
const char* FormatToken(ml::Token token) {
  switch (token) {
    case ml::Token::kSystem:
      return "S: ";
    case ml::Token::kUser:
      return "U: ";
    case ml::Token::kModel:
      return "M: ";
    default:
      NOTREACHED();
  }
}

// Convert an Input to a string for expectation matching.
std::string FormatInput(const on_device_model::mojom::Input& input) {
  std::string str;
  for (const auto& piece : input.pieces) {
    if (std::holds_alternative<ml::Token>(piece)) {
      str += FormatToken(std::get<ml::Token>(piece));
    } else if (std::holds_alternative<std::string>(piece)) {
      str += std::get<std::string>(piece);
    } else if (std::holds_alternative<SkBitmap>(piece)) {
      str += "<image>";
    } else if (std::holds_alternative<ml::AudioBuffer>(piece)) {
      str += "<audio>";
    }
  }
  return str;
}

// Convert a Context to string for expectation matching.
std::string GetContextString(AILanguageModel::Context& ctx) {
  return FormatInput(*ctx.GetNonInitialPrompts());
}

class TestStreamingResponder : public blink::mojom::ModelStreamingResponder {
 public:
  TestStreamingResponder() = default;
  ~TestStreamingResponder() override = default;

  mojo::PendingRemote<blink::mojom::ModelStreamingResponder> BindRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  // Returns true on successful completion and false on error.
  bool WaitForCompletion() {
    run_loop_.Run();
    return !error_status_.has_value();
  }

  void WaitForQuotaOverflow() { quota_overflow_run_loop_.Run(); }

  blink::mojom::ModelStreamingResponseStatus error_status() const {
    EXPECT_TRUE(error_status_.has_value());
    return *error_status_;
  }

  blink::mojom::QuotaErrorInfo quota_error_info() const {
    return *quota_error_info_;
  }

  const std::vector<std::string> responses() const { return responses_; }
  uint64_t current_tokens() const { return current_tokens_; }

 private:
  // blink::mojom::ModelStreamingResponder:
  void OnError(blink::mojom::ModelStreamingResponseStatus status,
               blink::mojom::QuotaErrorInfoPtr quota_error_info) override {
    error_status_ = status;
    quota_error_info_ = std::move(quota_error_info);
    run_loop_.Quit();
  }

  void OnStreaming(const std::string& text) override {
    responses_.push_back(text);
  }

  void OnCompletion(
      blink::mojom::ModelExecutionContextInfoPtr context_info) override {
    current_tokens_ = context_info->current_tokens;
    run_loop_.Quit();
  }

  void OnQuotaOverflow() override { quota_overflow_run_loop_.Quit(); }

  std::optional<blink::mojom::ModelStreamingResponseStatus> error_status_;
  blink::mojom::QuotaErrorInfoPtr quota_error_info_;
  std::vector<std::string> responses_;
  uint64_t current_tokens_ = 0;
  base::RunLoop run_loop_;
  base::RunLoop quota_overflow_run_loop_;
  mojo::Receiver<blink::mojom::ModelStreamingResponder> receiver_{this};
};

optimization_guide::proto::OnDeviceModelExecutionFeatureConfig CreateConfig() {
  optimization_guide::proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  optimization_guide::proto::SamplingParams sampling_params;
  sampling_params.set_top_k(kTestMaxTopK);
  sampling_params.set_temperature(kTestMaxTemperature);
  *config.mutable_sampling_params() = sampling_params;

  config.mutable_input_config()->set_max_context_tokens(kTestMaxTokens);

  optimization_guide::proto::PromptApiMetadata metadata;
  *metadata.mutable_max_sampling_params() = sampling_params;
  *config.mutable_feature_metadata() =
      optimization_guide::AnyWrapProto(metadata);

  config.set_feature(optimization_guide::proto::ModelExecutionFeature::
                         MODEL_EXECUTION_FEATURE_PROMPT_API);
  return config;
}

// Formats responses to match what the fake on device model service will return.
// The fake service keeps track of all previous inputs to a session, and will
// spit them all back out during a Generate() call. This gets a bit complicated
// for the language model, which also adds back the output as input to the
// session. An example language model session using the default behavior of the
// fake service would look something like this:
// - s1.Prompt("foo")
//   - Adds "UfooEM" to the session
//   - Gets output of ["UfooEM"] from fake service
//   - Adds "UfooEME" to the session (fake response + end token)
// - s1.Prompt("bar")
//   - Adds "UbarEM" to the session
//   - Gets output of ["UfooEM", "UfooEME", "UbarEM"].
//   - Adds "UfooEMUfooEMEUbarEM"
//     (concatenated output from fake service) to the session
// This behavior verifies the correct inputs and outputs are being returned from
// the model, and this helper makes it easier to construct these expectations.
// TODO(crbug.com/415808003): Simplify this in the fake service.
std::vector<std::string> FormatResponses(
    const std::vector<std::string>& responses) {
  std::vector<std::string> formatted;
  std::string last_output;
  for (const std::string& response : responses) {
    if (!last_output.empty()) {
      formatted.push_back(last_output + "E");
      last_output += formatted.back();
    }
    formatted.push_back(response);
    last_output += formatted.back();
  }
  return formatted;
}

class AILanguageModelTest : public AITestUtils::AITestBase {
 public:
  AILanguageModelTest()
      : fake_broker_(optimization_guide::FakeAdaptationAsset(
            {.config = CreateConfig()})) {}

  void SetUp() override {
    AITestBase::SetUp();
    SetupMockOptimizationGuideKeyedService();
    ai_manager_ =
        std::make_unique<AIManager>(main_rfh()->GetBrowserContext(),
                                    &component_update_service_, main_rfh());
  }

 protected:
  void SetupMockOptimizationGuideKeyedService() override {
    AITestUtils::AITestBase::SetupMockOptimizationGuideKeyedService();
    ON_CALL(*mock_optimization_guide_keyed_service_, CreateModelBrokerClient())
        .WillByDefault([&]() {
          return std::make_unique<optimization_guide::ModelBrokerClient>(
              fake_broker_.BindAndPassRemote(),
              optimization_guide::CreateSessionArgs(nullptr, {}));
        });
    ON_CALL(*mock_optimization_guide_keyed_service_,
            GetSamplingParamsConfig(
                optimization_guide::ModelBasedCapabilityKey::kPromptApi))
        .WillByDefault([]() {
          return optimization_guide::SamplingParamsConfig{
              .default_top_k = kTestDefaultTopK,
              .default_temperature = kTestDefaultTemperature};
        });
    ON_CALL(*mock_optimization_guide_keyed_service_,
            GetFeatureMetadata(
                optimization_guide::ModelBasedCapabilityKey::kPromptApi))
        .WillByDefault([]() { return CreateConfig().feature_metadata(); });
    ON_CALL(*mock_optimization_guide_keyed_service_, GetOnDeviceCapabilities())
        .WillByDefault(Return(on_device_model::Capabilities(
            {on_device_model::CapabilityFlags::kImageInput,
             on_device_model::CapabilityFlags::kAudioInput})));
    ON_CALL(*mock_optimization_guide_keyed_service_,
            GetOnDeviceModelEligibility(_))
        .WillByDefault(Return(
            optimization_guide::OnDeviceModelEligibilityReason::kSuccess));
  }

  mojo::Remote<blink::mojom::AILanguageModel> CreateSession(
      blink::mojom::AILanguageModelCreateOptionsPtr options =
          blink::mojom::AILanguageModelCreateOptions::New()) {
    base::test::TestFuture<mojo::Remote<blink::mojom::AILanguageModel>> future;
    AITestUtils::MockCreateLanguageModelClient language_model_client;
    EXPECT_CALL(language_model_client, OnResult(_, _))
        .WillOnce([&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                          language_model,
                      blink::mojom::AILanguageModelInstanceInfoPtr info) {
          future.SetValue(mojo::Remote<blink::mojom::AILanguageModel>(
              std::move(language_model)));
        });

    GetAIManagerRemote()->CreateLanguageModel(
        language_model_client.BindNewPipeAndPassRemote(), std::move(options));
    return future.Take();
  }

  std::vector<std::string> Prompt(
      blink::mojom::AILanguageModel& model,
      std::vector<blink::mojom::AILanguageModelPromptPtr> input,
      on_device_model::mojom::ResponseConstraintPtr constraint = nullptr) {
    TestStreamingResponder responder;
    model.Prompt(std::move(input), std::move(constraint),
                 responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    return responder.responses();
  }

  void Append(blink::mojom::AILanguageModel& model,
              std::vector<blink::mojom::AILanguageModelPromptPtr> input) {
    TestStreamingResponder responder;
    model.Append(std::move(input), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
  }

  mojo::Remote<blink::mojom::AILanguageModel> Fork(
      blink::mojom::AILanguageModel& model) {
    base::test::TestFuture<mojo::Remote<blink::mojom::AILanguageModel>> future;
    AITestUtils::MockCreateLanguageModelClient language_model_client;
    EXPECT_CALL(language_model_client, OnResult(_, _))
        .WillOnce([&](mojo::PendingRemote<blink::mojom::AILanguageModel>
                          language_model,
                      blink::mojom::AILanguageModelInstanceInfoPtr info) {
          future.SetValue(mojo::Remote<blink::mojom::AILanguageModel>(
              std::move(language_model)));
        });

    model.Fork(language_model_client.BindNewPipeAndPassRemote());
    return future.Take();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      blink::features::kAIPromptAPIMultimodalInput};
  optimization_guide::FakeModelBroker fake_broker_;
};

TEST_F(AILanguageModelTest, MultiplePrompts) {
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAreArray(FormatResponses({"UfooEM"})));
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAreArray(FormatResponses({"UfooEM", "UbarEM"})));
  EXPECT_THAT(
      Prompt(*session, MakeInput("baz")),
      ElementsAreArray(FormatResponses({"UfooEM", "UbarEM", "UbazEM"})));
}

TEST_F(AILanguageModelTest, Append) {
  auto session = CreateSession();
  Append(*session, MakeInput("foo"));
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("UfooE", "UbarEM"));
}

TEST_F(AILanguageModelTest, PromptTokenCounts) {
  fake_broker_.settings().set_execute_result({"hi"});
  auto session = CreateSession();

  std::string expected_tokens = "UfooEMhiE";
  {
    TestStreamingResponder responder;
    session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  expected_tokens += "UbarEMhiE";
  {
    TestStreamingResponder responder;
    session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  auto fork = Fork(*session);
  expected_tokens += "UbazEMhiE";
  {
    TestStreamingResponder responder;
    fork->Prompt(MakeInput("baz"), nullptr, responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
}

TEST_F(AILanguageModelTest, AppendTokenCounts) {
  auto session = CreateSession();

  std::string expected_tokens = "UfooE";
  {
    TestStreamingResponder responder;
    session->Append(MakeInput("foo"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  expected_tokens += "UbarE";
  {
    TestStreamingResponder responder;
    session->Append(MakeInput("bar"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
  auto fork = Fork(*session);
  expected_tokens += "UbazE";
  {
    TestStreamingResponder responder;
    fork->Append(MakeInput("baz"), responder.BindRemote());
    EXPECT_TRUE(responder.WaitForCompletion());
    EXPECT_EQ(responder.current_tokens(), expected_tokens.size());
  }
}

TEST_F(AILanguageModelTest, Roles) {
  auto session = CreateSession();
  std::vector<blink::mojom::AILanguageModelPromptPtr> prompts;
  prompts.push_back(MakePrompt(Role::kUser, "user"));
  prompts.push_back(MakePrompt(Role::kSystem, "system"));
  prompts.push_back(MakePrompt(Role::kAssistant, "model"));
  EXPECT_THAT(Prompt(*session, std::move(prompts)),
              ElementsAreArray(FormatResponses({"UuserESsystemEMmodelEM"})));
}

TEST_F(AILanguageModelTest, Fork) {
  auto session = CreateSession();
  auto fork1 = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAreArray(FormatResponses({"UfooEM"})));
  auto fork2 = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAreArray(FormatResponses({"UfooEM", "UbarEM"})));
  auto fork3 = Fork(*session);

  EXPECT_THAT(Prompt(*fork1, MakeInput("fork")),
              ElementsAreArray(FormatResponses({"UforkEM"})));
  EXPECT_THAT(Prompt(*fork2, MakeInput("fork")),
              ElementsAreArray(FormatResponses({"UfooEM", "UforkEM"})));
  auto fork4 = Fork(*fork2);
  EXPECT_THAT(
      Prompt(*fork3, MakeInput("fork")),
      ElementsAreArray(FormatResponses({"UfooEM", "UbarEM", "UforkEM"})));
  EXPECT_THAT(
      Prompt(*session, MakeInput("baz")),
      ElementsAreArray(FormatResponses({"UfooEM", "UbarEM", "UbazEM"})));

  EXPECT_THAT(
      Prompt(*fork4, MakeInput("more")),
      ElementsAreArray(FormatResponses({"UfooEM", "UforkEM", "UmoreEM"})));
}

TEST_F(AILanguageModelTest, SamplingParams) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 2;
  sampling_params->temperature = 1.0;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));
  auto fork = Fork(*session);

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 2, Temp: 1"));
  EXPECT_THAT(Prompt(*fork, MakeInput("bar")),
              ElementsAre("UbarEM", "TopK: 2, Temp: 1"));
}

TEST_F(AILanguageModelTest, SamplingParamsTopKOutOfRange) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 0;
  sampling_params->temperature = 1.5f;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 1, Temp: 1.5"));
}

TEST_F(AILanguageModelTest, SamplingParamsTemperatureOutOfRange) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = 2;
  sampling_params->temperature = -1.0f;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 2, Temp: 0"));
}

TEST_F(AILanguageModelTest, MaxSamplingParams) {
  auto sampling_params = blink::mojom::AILanguageModelSamplingParams::New();
  sampling_params->top_k = kTestMaxTopK + 1;
  sampling_params->temperature = kTestMaxTemperature + 1;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->sampling_params = std::move(sampling_params);
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("UfooEM", "TopK: 5, Temp: 1.5"));
}

TEST_F(AILanguageModelTest, InitialPrompts) {
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));
  options->initial_prompts.push_back(MakePrompt(Role::kUser, "bye"));
  auto session = CreateSession(std::move(options));

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("ShiEUbyeE", "UfooEM"));
}

TEST_F(AILanguageModelTest, InitialPromptsInstanceInfo) {
  base::test::TestFuture<blink::mojom::AILanguageModelInstanceInfoPtr> future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnResult(_, _))
      .WillOnce(
          [&](mojo::PendingRemote<blink::mojom::AILanguageModel> language_model,
              blink::mojom::AILanguageModelInstanceInfoPtr info) {
            future.SetValue(std::move(info));
          });

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "hi"));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  auto info = future.Take();
  EXPECT_EQ(info->input_quota, kTestMaxTokens);
  EXPECT_EQ(info->input_usage, std::strlen("ShiE"));
}

TEST_F(AILanguageModelTest, InitialPromptsTooLarge) {
  base::test::TestFuture<blink::mojom::AIManagerCreateClientError> error_future;
  base::test::TestFuture<blink::mojom::QuotaErrorInfoPtr>
      quota_error_info_future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnError(_, _))
      .WillOnce([&](auto error, auto quota_error_info) {
        error_future.SetValue(error);
        quota_error_info_future.SetValue(std::move(quota_error_info));
      });

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(
      MakePrompt(Role::kSystem, std::string(kTestMaxTokens + 1, 'a')));

  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  EXPECT_EQ(error_future.Take(),
            blink::mojom::AIManagerCreateClientError::kInitialInputTooLarge);
  auto quota_error_info = quota_error_info_future.Take();
  ASSERT_TRUE(quota_error_info);
  ASSERT_GT(quota_error_info->requested, kTestMaxTokens);
  ASSERT_EQ(quota_error_info->quota, kTestMaxTokens);
}

TEST_F(AILanguageModelTest, InputTooLarge) {
  auto session = CreateSession();

  TestStreamingResponder responder;
  session->Prompt(MakeInput(std::string(kTestMaxTokens + 1, 'a')), nullptr,
                  responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorInputTooLarge);
  ASSERT_GT(responder.quota_error_info().requested, kTestMaxTokens);
  ASSERT_EQ(responder.quota_error_info().quota, kTestMaxTokens);
}

TEST_F(AILanguageModelTest, QuotaOverflowOnPromptInput) {
  // Set the execute result so the long prompt is not echoed back as the
  // response.
  fake_broker_.settings().set_execute_result({"hi"});
  // Initial prompt should be kept on overflow.
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "init"));
  auto session = CreateSession(std::move(options));
  // Set a prompt that is close to max token length. This string should be
  // stripped from the prompt history, while the initial prompts and
  // `long_prompt` will be kept.
  EXPECT_THAT(
      Prompt(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a'))),
      ElementsAre("hi"));
  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("hi"));

  // Clear execute result so we can verify the input by checking the response.
  fake_broker_.settings().set_execute_result({});
  std::string long_prompt(kTestMaxTokens / 3, 'a');
  TestStreamingResponder responder;
  session->Prompt(MakeInput(long_prompt), nullptr, responder.BindRemote());
  responder.WaitForQuotaOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());
  // Response should include input/output of previous prompt with the original
  // long prompt not present.
  EXPECT_THAT(responder.responses(),
              ElementsAre("SinitE", "UfooEMhiE", "U" + long_prompt + "EM"));
}

TEST_F(AILanguageModelTest, QuotaOverflowOnAppend) {
  // Initial prompt should be kept on overflow.
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "init"));
  auto session = CreateSession(std::move(options));
  // Set a prompt that is close to max token length.
  Append(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a')));

  std::string long_prompt(kTestMaxTokens / 3, 'a');
  TestStreamingResponder responder;
  session->Append(MakeInput(long_prompt), responder.BindRemote());
  responder.WaitForQuotaOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());

  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAre("SinitE", "U" + long_prompt + "E", "UfooEM"));
}

TEST_F(AILanguageModelTest, QuotaOverflowOnOutput) {
  // Set the execute result so the long prompt is not echoed back as the
  // response.
  fake_broker_.settings().set_execute_result({"hi"});
  auto session = CreateSession();
  // Set a prompt that is close to max token length. This string should be
  // stripped from the prompt history, while the next prompt's input and output
  // will be kept.
  EXPECT_THAT(
      Prompt(*session, MakeInput(std::string(kTestMaxTokens - 20, 'a'))),
      ElementsAre("hi"));

  // Reset result to a long response that should cause overflow. `long_response`
  // should be kept, but the previous prompt will be removed.
  std::string long_response(kTestMaxTokens / 3, 'a');
  fake_broker_.settings().set_execute_result({long_response});

  TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  responder.WaitForQuotaOverflow();
  EXPECT_TRUE(responder.WaitForCompletion());
  EXPECT_THAT(responder.responses(), ElementsAre(long_response));

  // Verify the original long response was removed. The response should contain:
  // - "foo"+long_response from the previous prompt call
  // - "bar" from the current prompt call
  fake_broker_.settings().set_execute_result({});
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("UfooEM" + long_response + "E", "UbarEM"));
}

TEST_F(AILanguageModelTest, Destroy) {
  auto session = CreateSession();
  base::RunLoop run_loop;
  session.set_disconnect_handler(run_loop.QuitClosure());
  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAreArray(FormatResponses({"UfooEM"})));
  session->Destroy();
  run_loop.Run();
}

TEST_F(AILanguageModelTest, DestroyWithActivePrompt) {
  fake_broker_.settings().set_execute_delay(base::Minutes(1));
  auto session = CreateSession();
  base::RunLoop run_loop;
  session.set_disconnect_handler(run_loop.QuitClosure());

  TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  session->Destroy();
  run_loop.Run();

  EXPECT_FALSE(responder.WaitForCompletion());
}

TEST_F(AILanguageModelTest, UnsupportedLanguage) {
  base::test::TestFuture<blink::mojom::AIManagerCreateClientError> future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnError(_, _))
      .WillOnce(
          [&](auto error, auto quota_error_info) { future.SetValue(error); });

  auto expected_input = blink::mojom::AILanguageModelExpected::New();
  expected_input->languages.emplace();
  expected_input->languages->push_back(blink::mojom::AILanguageCode::New("ja"));

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(expected_input));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  EXPECT_EQ(future.Take(),
            blink::mojom::AIManagerCreateClientError::kUnsupportedLanguage);
}

TEST_F(AILanguageModelTest, UnsupportedInputCapability) {
  ON_CALL(*mock_optimization_guide_keyed_service_, GetOnDeviceCapabilities())
      .WillByDefault(Return(on_device_model::Capabilities()));

  base::test::TestFuture<blink::mojom::AIManagerCreateClientError> future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnError(_, _))
      .WillOnce(
          [&](auto error, auto quota_error_info) { future.SetValue(error); });

  auto expected_input = blink::mojom::AILanguageModelExpected::New();
  expected_input->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(expected_input));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  EXPECT_EQ(future.Take(),
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, UnsupportedOutputCapability) {
  ON_CALL(*mock_optimization_guide_keyed_service_, GetOnDeviceCapabilities())
      .WillByDefault(Return(on_device_model::Capabilities()));

  base::test::TestFuture<blink::mojom::AIManagerCreateClientError> future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnError(_, _))
      .WillOnce(
          [&](auto error, auto quota_error_info) { future.SetValue(error); });

  auto expected_output = blink::mojom::AILanguageModelExpected::New();
  expected_output->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(std::move(expected_output));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  EXPECT_EQ(future.Take(),
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, MultimodalInputImageNotSpecified) {
  auto audio_input = blink::mojom::AILanguageModelExpected::New();
  audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(audio_input));
  auto session = CreateSession(std::move(options));

  auto make_input = [] {
    std::vector<blink::mojom::AILanguageModelPromptPtr> input =
        MakeInput("foo");
    input.push_back(blink::mojom::AILanguageModelPrompt::New(
        Role::kUser,
        ToVector(blink::mojom::AILanguageModelPromptContent::NewBitmap(
            CreateTestBitmap(10, 10)))));
    return input;
  };
  {
    TestStreamingResponder responder;
    session->Prompt(make_input(), nullptr, responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  {
    TestStreamingResponder responder;
    session->Append(make_input(), responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(make_input(), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::nullopt);
}

TEST_F(AILanguageModelTest, MultimodalInputAudioNotSpecified) {
  auto image_input = blink::mojom::AILanguageModelExpected::New();
  image_input->type = blink::mojom::AILanguageModelPromptType::kImage;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(image_input));
  auto session = CreateSession(std::move(options));

  auto make_input = [] {
    std::vector<blink::mojom::AILanguageModelPromptPtr> input =
        MakeInput("foo");
    input.push_back(blink::mojom::AILanguageModelPrompt::New(
        Role::kUser,
        ToVector(blink::mojom::AILanguageModelPromptContent::NewAudio(
            CreateTestAudio()))));
    return input;
  };
  {
    TestStreamingResponder responder;
    session->Prompt(make_input(), nullptr, responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  {
    TestStreamingResponder responder;
    session->Append(make_input(), responder.BindRemote());
    EXPECT_FALSE(responder.WaitForCompletion());
    EXPECT_EQ(responder.error_status(),
              blink::mojom::ModelStreamingResponseStatus::kErrorInvalidRequest);
  }
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(make_input(), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::nullopt);
}

TEST_F(AILanguageModelTest, MultimodalInput) {
  auto audio_input = blink::mojom::AILanguageModelExpected::New();
  audio_input->type = blink::mojom::AILanguageModelPromptType::kAudio;
  auto image_input = blink::mojom::AILanguageModelExpected::New();
  image_input->type = blink::mojom::AILanguageModelPromptType::kImage;

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(std::move(audio_input));
  options->expected_inputs->push_back(std::move(image_input));
  auto session = CreateSession(std::move(options));

  std::vector<blink::mojom::AILanguageModelPromptPtr> input = MakeInput("foo");
  input.push_back(blink::mojom::AILanguageModelPrompt::New(
      Role::kUser,
      ToVector(blink::mojom::AILanguageModelPromptContent::NewBitmap(
          CreateTestBitmap(10, 10)))));
  input.push_back(blink::mojom::AILanguageModelPrompt::New(
      Role::kUser,
      ToVector(blink::mojom::AILanguageModelPromptContent::NewAudio(
          CreateTestAudio()))));
  EXPECT_THAT(Prompt(*session, std::move(input)),
              ElementsAreArray(FormatResponses({"UfooEU<image>EU<audio>EM"})));
}

TEST_F(AILanguageModelTest, ModelDownload) {
  EXPECT_EQ(GetAIManagerDownloadProgressObserversSize(), 0u);
  AITestUtils::FakeMonitor mock_monitor;

  EXPECT_CALL(component_update_service_, GetComponentIDs()).Times(1);
  GetAIManagerRemote()->AddModelDownloadProgressObserver(
      mock_monitor.BindNewPipeAndPassRemote());

  ASSERT_TRUE(base::test::RunUntil(
      [this] { return GetAIManagerDownloadProgressObserversSize() == 1u; }));

  // This is the component id of the on device model. The `AIManager` sends
  // updates for it to the `CreateMonitor`s.
  std::string model_component_id =
      component_updater::OptimizationGuideOnDeviceModelInstallerPolicy::
          GetOnDeviceModelExtensionId();
  AITestUtils::FakeComponent model_component(model_component_id,
                                             kTestModelDownloadSize);

  component_update_service_.SendUpdate(model_component.CreateUpdateItem(
      update_client::ComponentState::kDownloading, kTestModelDownloadSize));

  mock_monitor.ExpectReceivedNormalizedUpdate(0, kTestModelDownloadSize);
  mock_monitor.ExpectReceivedNormalizedUpdate(kTestModelDownloadSize,
                                              kTestModelDownloadSize);
}

TEST_F(AILanguageModelTest, MeasureInputUsage) {
  auto session = CreateSession();
  base::test::TestFuture<std::optional<uint32_t>> measure_future;
  session->MeasureInputUsage(MakeInput("foo"), measure_future.GetCallback());
  EXPECT_EQ(measure_future.Get(), std::string("UfooEM").size());
}

TEST_F(AILanguageModelTest, TextSafetyInitialPrompts) {
  auto config = CreateConfig();
  config.set_can_skip_text_safety(false);
  fake_broker_.UpdateModelAdaptation(
      optimization_guide::FakeAdaptationAsset({.config = config}));
  auto safety_config = CreateSafetyConfig();
  auto* check = safety_config.add_request_check();
  check->mutable_input_template()->Add(
      FieldSubstitution("%s", StringValueField()));
  optimization_guide::FakeSafetyModelAsset safety_asset(
      std::move(safety_config));
  fake_broker_.UpdateSafetyModel(safety_asset.model_info());

  base::test::TestFuture<blink::mojom::AIManagerCreateClientError> future;
  AITestUtils::MockCreateLanguageModelClient language_model_client;
  EXPECT_CALL(language_model_client, OnError(_, _))
      .WillOnce(
          [&](auto error, auto quota_error_info) { future.SetValue(error); });

  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->initial_prompts.push_back(MakePrompt(Role::kSystem, "unsafe"));
  GetAIManagerRemote()->CreateLanguageModel(
      language_model_client.BindNewPipeAndPassRemote(), std::move(options));
  EXPECT_EQ(future.Take(),
            blink::mojom::AIManagerCreateClientError::kUnableToCreateSession);
}

TEST_F(AILanguageModelTest, TextSafetyInput) {
  auto config = CreateConfig();
  config.set_can_skip_text_safety(false);
  fake_broker_.UpdateModelAdaptation(
      optimization_guide::FakeAdaptationAsset({.config = config}));
  auto safety_config = CreateSafetyConfig();
  auto* check = safety_config.add_request_check();
  check->mutable_input_template()->Add(
      FieldSubstitution("%s", StringValueField()));
  optimization_guide::FakeSafetyModelAsset safety_asset(
      std::move(safety_config));
  fake_broker_.UpdateSafetyModel(safety_asset.model_info());

  fake_broker_.settings().set_execute_result({"hi"});
  auto session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("safe")), ElementsAre("hi"));

  // Fake text safety checker looks for the string "unsafe".
  TestStreamingResponder responder;
  session->Prompt(MakeInput("unsafe"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
}

TEST_F(AILanguageModelTest, TextSafetyOutput) {
  auto config = CreateConfig();
  config.set_can_skip_text_safety(false);
  fake_broker_.UpdateModelAdaptation(
      optimization_guide::FakeAdaptationAsset({.config = config}));
  auto safety_config = CreateSafetyConfig();
  auto* check = safety_config.mutable_raw_output_check();
  check->mutable_input_template()->Add(
      FieldSubstitution("%s", StringValueField()));
  safety_config.mutable_partial_output_checks()->set_minimum_tokens(1000);
  optimization_guide::FakeSafetyModelAsset safety_asset(
      std::move(safety_config));
  fake_broker_.UpdateSafetyModel(safety_asset.model_info());

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_.settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto session = CreateSession();
  TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  EXPECT_TRUE(responder.responses().empty());
}

TEST_F(AILanguageModelTest, TextSafetyOutputPartial) {
  auto config = CreateConfig();
  config.set_can_skip_text_safety(false);
  fake_broker_.UpdateModelAdaptation(
      optimization_guide::FakeAdaptationAsset({.config = config}));
  auto safety_config = CreateSafetyConfig();
  auto* check = safety_config.mutable_raw_output_check();
  check->mutable_input_template()->Add(
      FieldSubstitution("%s", StringValueField()));
  safety_config.mutable_partial_output_checks()->set_minimum_tokens(3);
  safety_config.mutable_partial_output_checks()->set_token_interval(2);
  optimization_guide::FakeSafetyModelAsset safety_asset(
      std::move(safety_config));
  fake_broker_.UpdateSafetyModel(safety_asset.model_info());

  // Fake text safety checker looks for the string "unsafe".
  fake_broker_.settings().set_execute_result(
      {"a", "b", "c", "d", "e", "f", "g", "unsafe", "h"});
  auto session = CreateSession();
  TestStreamingResponder responder;
  session->Prompt(MakeInput("foo"), nullptr, responder.BindRemote());
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorFiltered);
  // Partial checks should still allow some output to stream.
  EXPECT_THAT(responder.responses(), ElementsAre("abc", "de", "fg"));
}

TEST_F(AILanguageModelTest, QueuesOperations) {
  base::test::TestFuture<mojo::Remote<blink::mojom::AILanguageModel>>
      fork_future;
  AITestUtils::MockCreateLanguageModelClient fork_client;
  EXPECT_CALL(fork_client, OnResult(_, _))
      .WillOnce([&](auto language_model, auto info) {
        fork_future.SetValue(mojo::Remote<blink::mojom::AILanguageModel>(
            std::move(language_model)));
      });

  auto session = CreateSession();
  TestStreamingResponder responder1;
  TestStreamingResponder responder2;
  TestStreamingResponder responder3;
  // Add three prompts and a fork, all these operations should complete
  // successfully and in order.
  session->Prompt(MakeInput("foo"), nullptr, responder1.BindRemote());
  session->Prompt(MakeInput("bar"), nullptr, responder2.BindRemote());
  session->Fork(fork_client.BindNewPipeAndPassRemote());
  session->Prompt(MakeInput("baz"), nullptr, responder3.BindRemote());

  EXPECT_TRUE(responder1.WaitForCompletion());
  EXPECT_THAT(responder1.responses(),
              ElementsAreArray(FormatResponses({"UfooEM"})));

  EXPECT_TRUE(responder2.WaitForCompletion());
  EXPECT_THAT(responder2.responses(),
              ElementsAreArray(FormatResponses({"UfooEM", "UbarEM"})));

  EXPECT_TRUE(responder3.WaitForCompletion());
  EXPECT_THAT(
      responder3.responses(),
      ElementsAreArray(FormatResponses({"UfooEM", "UbarEM", "UbazEM"})));

  EXPECT_THAT(
      Prompt(*fork_future.Take(), MakeInput("fork")),
      ElementsAreArray(FormatResponses({"UfooEM", "UbarEM", "UforkEM"})));
}

TEST_F(AILanguageModelTest, Constraint) {
  auto session = CreateSession();
  EXPECT_THAT(
      Prompt(*session, MakeInput("foo"),
             on_device_model::mojom::ResponseConstraint::NewRegex("reg")),
      ElementsAre("Constraint: regex reg", "UfooEM"));
}

TEST_F(AILanguageModelTest, ServiceCrash) {
  auto session = CreateSession();
  TestStreamingResponder responder;
  session->Prompt(MakeInput("bar"), nullptr, responder.BindRemote());
  fake_broker_.CrashService();
  EXPECT_FALSE(responder.WaitForCompletion());
  EXPECT_EQ(responder.error_status(),
            blink::mojom::ModelStreamingResponseStatus::kErrorGenericFailure);

  // Recreating the session should be fine.
  session = CreateSession();
  EXPECT_THAT(Prompt(*session, MakeInput("foo")),
              ElementsAreArray(FormatResponses({"UfooEM"})));
}

// TODO(crbug.com/414632884): This test is flaky on Linux TSAN.
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
#define MAYBE_CanCreate_WaitsForEligibility \
  DISABLED_CanCreate_WaitsForEligibility
#else
#define MAYBE_CanCreate_WaitsForEligibility CanCreate_WaitsForEligibility
#endif
TEST_F(AILanguageModelTest, MAYBE_CanCreate_WaitsForEligibility) {
  base::test::TestFuture<base::OnceCallback<void(
      optimization_guide::OnDeviceModelEligibilityReason)>>
      eligibility_future;
  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              GetOnDeviceModelEligibilityAsync(_, _, _))
      .WillOnce(
          testing::Invoke([&](auto feature, auto capabilities, auto callback) {
            eligibility_future.SetValue(std::move(callback));
          }));

  base::test::TestFuture<blink::mojom::ModelAvailabilityCheckResult>
      result_future;
  GetAIManagerInterface()->CanCreateLanguageModel({},
                                                  result_future.GetCallback());
  // Session should not be ready until eligibility callback has run.
  EXPECT_FALSE(result_future.IsReady());
  eligibility_future.Take().Run(
      optimization_guide::OnDeviceModelEligibilityReason::kSuccess);
  EXPECT_EQ(result_future.Get(),
            blink::mojom::ModelAvailabilityCheckResult::kAvailable);
}

TEST_F(AILanguageModelTest, CanCreate_SupportedLanguages) {
  base::MockCallback<AIManager::CanCreateLanguageModelCallback> callback;
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"en"})));
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"en"})));
  EXPECT_CALL(callback,
              Run(blink::mojom::ModelAvailabilityCheckResult::kAvailable));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  callback.Get());
}

TEST_F(AILanguageModelTest, CanCreate_UnsupportedInputLanguages) {
  base::MockCallback<AIManager::CanCreateLanguageModelCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_inputs.emplace();
  options->expected_inputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"ja"})));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  callback.Get());
}

TEST_F(AILanguageModelTest, CanCreate_UnsupportedOutputLanguages) {
  base::MockCallback<AIManager::CanCreateLanguageModelCallback> callback;
  EXPECT_CALL(callback, Run(blink::mojom::ModelAvailabilityCheckResult::
                                kUnavailableUnsupportedLanguage));
  auto options = blink::mojom::AILanguageModelCreateOptions::New();
  options->expected_outputs.emplace();
  options->expected_outputs->push_back(
      blink::mojom::AILanguageModelExpected::New(
          blink::mojom::AILanguageModelPromptType::kText,
          AITestUtils::ToMojoLanguageCodes({"ja"})));
  GetAIManagerInterface()->CanCreateLanguageModel(std::move(options),
                                                  callback.Get());
}

// Tests the `AILanguageModel::Context` that's initialized with/without any
// initial prompt.
class AILanguageModelContextTest : public testing::Test {
 public:
  AILanguageModel::Context context_{kTestMaxContextToken};
};

// Tests `GetContextString()` when the context is empty.
TEST_F(AILanguageModelContextTest, TestContextOperation_Empty) {
  EXPECT_EQ(GetContextString(context_), "");
}

// Tests `GetContextString()` when some items are added to the context.
TEST_F(AILanguageModelContextTest, TestContextOperation_NonEmpty) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: test");

  context_.AddContextItem(SimpleContextItem(" test again", 2u));
  EXPECT_EQ(GetContextString(context_), "S: testS:  test again");
}

// Tests `GetContextString()` when the items overflow.
TEST_F(AILanguageModelContextTest, TestContextOperation_Overflow) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("test", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: test");

  // Since the total number of tokens will exceed `kTestMaxContextToken`, the
  // old item will be evicted.
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("long token", kTestMaxContextToken)),
      AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable);
  EXPECT_EQ(GetContextString(context_), "S: long token");
}

TEST_F(AILanguageModelContextTest, TestContextOperation_PartialOverflow) {
  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("foo", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: foo");

  EXPECT_EQ(context_.AddContextItem(SimpleContextItem("bar", 1u)),
            AILanguageModel::Context::SpaceReservationResult::kSufficientSpace);
  EXPECT_EQ(GetContextString(context_), "S: fooS: bar");

  // Add 1 token less than `kTestMaxContextToken` so one of the previous items
  // will be kept.
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("long token", kTestMaxContextToken - 1)),
      AILanguageModel::Context::SpaceReservationResult::kSpaceMadeAvailable);
  EXPECT_EQ(GetContextString(context_), "S: barS: long token");
}

// Tests `GetContextString()` when the items overflow on the first insertion.
TEST_F(AILanguageModelContextTest, TestContextOperation_OverflowOnFirstItem) {
  EXPECT_EQ(
      context_.AddContextItem(
          SimpleContextItem("test very long token", kTestMaxContextToken + 1u)),
      AILanguageModel::Context::SpaceReservationResult::kInsufficientSpace);
  EXPECT_EQ(GetContextString(context_), "");
}

TEST_F(AILanguageModelTest, Priority) {
  fake_broker_.settings().set_execute_result({"hi"});
  auto session = CreateSession();

  EXPECT_THAT(Prompt(*session, MakeInput("foo")), ElementsAre("hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Hide();
  EXPECT_THAT(Prompt(*session, MakeInput("bar")),
              ElementsAre("Priority: background", "hi"));

  auto fork = Fork(*session);
  EXPECT_THAT(Prompt(*fork, MakeInput("bar")),
              ElementsAre("Priority: background", "hi"));

  main_rfh()->GetRenderWidgetHost()->GetView()->Show();
  EXPECT_THAT(Prompt(*session, MakeInput("baz")), ElementsAre("hi"));
}

}  // namespace
