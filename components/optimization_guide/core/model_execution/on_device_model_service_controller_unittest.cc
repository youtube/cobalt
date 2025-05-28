// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/model_execution/model_execution_prefs.h"
#include "components/optimization_guide/core/model_execution/multimodal_message.h"
#include "components/optimization_guide/core/model_execution/on_device_execution.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_execution_proto_value_utils.h"
#include "components/optimization_guide/core/model_execution/on_device_model_metadata.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/request_builder.h"
#include "components/optimization_guide/core/model_execution/test/response_holder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/model_quality/test_model_quality_logs_uploader_service.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "components/optimization_guide/proto/features/example_for_testing.pb.h"
#include "components/optimization_guide/proto/model_execution.pb.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/redaction.pb.h"
#include "components/optimization_guide/proto/substitution.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/service_client.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

namespace {

using ::on_device_model::mojom::LoadModelResult;
using ExecuteModelResult = ::optimization_guide::OnDeviceExecution::Result;

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::ResultOf;

auto UnsafeComposeConfig() {
  auto cfg = SimpleComposeConfig();
  cfg.set_can_skip_text_safety(true);
  return cfg;
}

auto UnsafeTestConfig() {
  auto cfg = UnsafeComposeConfig();
  cfg.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
  return cfg;
}

// A complete set of assets for the most common case.
struct StandardAssets {
  FakeBaseModelAsset base_model{FakeBaseModelAsset::Content{}};
  FakeAdaptationAsset compose{{
      .config = SimpleComposeConfig(),
  }};
  FakeSafetyModelAsset safety{ComposeSafetyConfig()};
  FakeLanguageModelAsset language;
};

const std::string& GetCheckText(
    const proto::InternalOnDeviceModelExecutionInfo& log) {
  return log.request().text_safety_model_request().text();
}

// A remote fallback that is unsuccessful.
void BadRequestRemote(ModelBasedCapabilityKey key,
                      const google::protobuf::MessageLite& req,
                      std::optional<base::TimeDelta> timeout,
                      std::unique_ptr<proto::LogAiDataRequest> log,
                      OptimizationGuideModelExecutionResultCallback callback) {
  std::move(callback).Run(
      OptimizationGuideModelExecutionResult(
          base::unexpected(
              OptimizationGuideModelExecutionError::FromHttpStatusCode(
                  net::HTTP_BAD_REQUEST)),
          nullptr),
      nullptr);
}

// A remote callback that fails the test if it is used.
void FailRemote(ModelBasedCapabilityKey key,
                const google::protobuf::MessageLite& req,
                std::optional<base::TimeDelta> timeout,
                std::unique_ptr<proto::LogAiDataRequest> log,
                OptimizationGuideModelExecutionResultCallback callback) {
  EXPECT_TRUE(false) << "Unexpected use of remote fallback";
  BadRequestRemote(key, req, timeout, std::move(log), std::move(callback));
}

ExecuteRemoteFn FailOnRemoteFallback() {
  return base::BindRepeating(&FailRemote);
}

class FakeOnDeviceModelAvailabilityObserver
    : public OnDeviceModelAvailabilityObserver {
 public:
  explicit FakeOnDeviceModelAvailabilityObserver(
      ModelBasedCapabilityKey expected_feature) {
    expected_feature_ = expected_feature;
  }

  void OnDeviceModelAvailabilityChanged(
      ModelBasedCapabilityKey feature,
      OnDeviceModelEligibilityReason reason) override {
    EXPECT_EQ(expected_feature_, feature);
    reason_ = reason;
  }
  ModelBasedCapabilityKey expected_feature_;
  std::optional<OnDeviceModelEligibilityReason> reason_;
};

}  // namespace

std::vector<std::string> ConcatResponses(
    const std::vector<std::string>& responses) {
  std::vector<std::string> concat_responses;
  std::string current_response;
  for (const std::string& response : responses) {
    current_response += response;
    concat_responses.push_back(current_response);
  }
  return concat_responses;
}

constexpr auto kFeature = ModelBasedCapabilityKey::kCompose;

class ExpectedRemoteFallback final {
 public:
  struct FallbackArgs {
    ModelBasedCapabilityKey feature;
    std::unique_ptr<google::protobuf::MessageLite> request;
    std::optional<base::TimeDelta> timeout;
    std::unique_ptr<proto::LogAiDataRequest> log;
    OptimizationGuideModelExecutionResultCallback callback;

    const auto& logged_executions() {
      return log->model_execution_info()
          .on_device_model_execution_info()
          .execution_infos();
    }
  };

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [&](ModelBasedCapabilityKey feature,
            const google::protobuf::MessageLite& m,
            std::optional<base::TimeDelta> t,
            std::unique_ptr<proto::LogAiDataRequest> l,
            OptimizationGuideModelExecutionResultCallback c) {
          auto request = base::WrapUnique(m.New());
          request->CheckTypeAndMergeFrom(m);
          future_.GetCallback().Run(FallbackArgs{
              feature,
              std::move(request),
              t,
              std::move(l),
              std::move(c),
          });
        });
  }

  proto::Any ComposeResponse(const std::string& output) {
    proto::ComposeResponse response;
    response.set_output(output);
    return AnyWrapProto(response);
  }

  FallbackArgs Take() { return future_.Take(); }

 private:
  base::test::TestFuture<FallbackArgs> future_;
};

class OnDeviceModelServiceControllerTest : public testing::Test {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationGuideModelExecution, {}},
         {features::kOptimizationGuideOnDeviceModel,
          {{"on_device_model_min_tokens_for_context", "10"},
           {"on_device_model_max_tokens_for_context", "22"},
           {"on_device_model_context_token_chunk_size", "4"},
           {"on_device_model_topk", "1"},
           {"on_device_model_temperature", "0"},
           {"on_device_model_disable_crash_count", "3"},
           {"on_device_model_crash_backoff_base_time", "1m"},
           {"on_device_model_max_crash_backoff_time", "1h"}}},
         {features::kOnDeviceModelPerformanceParams,
          {{"compatible_on_device_performance_classes", "*"},
           {"compatible_low_tier_on_device_performance_classes", "3"}}},
         {features::kTextSafetyClassifier, {}},
         {features::kOnDeviceModelValidation,
          {{"on_device_model_validation_delay", "0"}}}},
        {});
    model_execution::prefs::RegisterProfilePrefs(pref_service_.registry());
    model_execution::prefs::RegisterLocalStatePrefs(pref_service_.registry());

    // Fake the requirements to install the model.
    pref_service_.SetInteger(
        model_execution::prefs::localstate::kOnDevicePerformanceClass,
        base::to_underlying(OnDeviceModelPerformanceClass::kHigh));
    model_execution::prefs::RecordFeatureUsage(
        &pref_service_, ModelBasedCapabilityKey::kCompose);
  }

  void TearDown() override {
    access_controller_ = nullptr;
    test_controller_ = nullptr;
  }

  struct InitializeParams {
    raw_ptr<FakeBaseModelAsset> base_model;
    raw_ptr<FakeSafetyModelAsset> safety;
    raw_ptr<FakeLanguageModelAsset> language;
    std::vector<FakeAdaptationAsset*> adaptations;
  };

  void Initialize(const InitializeParams& params) {
    if (params.base_model) {
      on_device_component_state_manager_.get()->OnStartup();
      task_environment_.FastForwardBy(base::Seconds(1));
      on_device_component_state_manager_.SetReady(params.base_model->path(),
                                                  params.base_model->version());
    }
    RecreateServiceController();
    if (params.safety) {
      test_controller_->MaybeUpdateSafetyModel(params.safety->model_info());
    }
    if (params.language) {
      test_controller_->SetLanguageDetectionModel(
          params.language->model_info());
    }
    for (auto* adaptation : params.adaptations) {
      test_controller_->MaybeUpdateModelAdaptation(adaptation->feature(),
                                                   adaptation->metadata());
    }
    // Wait until the OnDeviceModelExecutionConfig has been read.
    task_environment_.RunUntilIdle();
  }

  void Initialize(StandardAssets& assets) {
    Initialize(InitializeParams{
        .base_model = &standard_assets_.base_model,
        .safety = &standard_assets_.safety,
        .language = &standard_assets_.language,
        .adaptations = {&standard_assets_.compose},
    });
  }

  ExecuteRemoteFn CreateExecuteRemoteFn() {
    return base::BindLambdaForTesting(
        [=, this](ModelBasedCapabilityKey feature,
                  const google::protobuf::MessageLite& m,
                  std::optional<base::TimeDelta> t,
                  std::unique_ptr<proto::LogAiDataRequest> l,
                  OptimizationGuideModelExecutionResultCallback c) {
          remote_execute_called_ = true;
          last_remote_message_ = base::WrapUnique(m.New());
          last_remote_message_->CheckTypeAndMergeFrom(m);
          log_ai_data_request_passed_to_remote_ = std::move(l);

          if (feature == ModelBasedCapabilityKey::kTextSafety) {
            last_remote_ts_callback_ = std::move(c);
          }
        });
  }

  void RecreateServiceController() {
    access_controller_ = nullptr;
    test_controller_ = nullptr;

    // Turn down the service, to simulate restart.
    fake_launcher_.CrashService();

    auto access_controller =
        std::make_unique<OnDeviceModelAccessController>(pref_service_);
    access_controller_ = access_controller.get();
    test_controller_ = base::MakeRefCounted<OnDeviceModelServiceController>(
        std::move(access_controller),
        on_device_component_state_manager_.get()->GetWeakPtr(),
        fake_launcher_.LaunchFn());

    test_controller_->Init();
  }

  std::map<ModelBasedCapabilityKey, OnDeviceModelAdaptationController>&
  GetModelAdaptationControllers() const {
    return test_controller_->model_adaptation_controllers_;
  }

  std::unique_ptr<OptimizationGuideModelExecutor::Session> CreateSession() {
    return test_controller_->CreateSession(kFeature, FailOnRemoteFallback(),
                                           logger_.GetWeakPtr(), nullptr,
                                           /*config_params=*/std::nullopt);
  }

  void ExpectFailedSession(OnDeviceModelEligibilityReason reason) {
    base::HistogramTester histogram_tester;
    EXPECT_FALSE(CreateSession());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        reason, 1);
  }

 protected:
  StandardAssets standard_assets_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  TestOnDeviceModelComponentStateManager on_device_component_state_manager_{
      &pref_service_};
  scoped_refptr<OnDeviceModelServiceController> test_controller_;
  // Owned by OnDeviceModelServiceController.
  raw_ptr<OnDeviceModelAccessController> access_controller_ = nullptr;
  ResponseHolder response_;
  base::test::ScopedFeatureList feature_list_;
  bool remote_execute_called_ = false;
  std::unique_ptr<google::protobuf::MessageLite> last_remote_message_;
  std::unique_ptr<proto::LogAiDataRequest>
      log_ai_data_request_passed_to_remote_;
  OptimizationGuideModelExecutionResultCallback last_remote_ts_callback_;
  OptimizationGuideLogger logger_;
};

TEST_F(OnDeviceModelServiceControllerTest, ScoreBeforeContext) {
  Initialize(standard_assets_);

  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_TRUE(session);
  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, ScorePresentAfterContext) {
  Initialize(standard_assets_);

  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_EQ(score_future.Get(), 0.5);
}

TEST_F(OnDeviceModelServiceControllerTest, ScoreAfterExecute) {
  Initialize(standard_assets_);

  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_TRUE(session);

  session->AddContext(UserInputRequest("foo"));
  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());

  base::test::TestFuture<std::optional<float>> score_future;
  session->Score("token", score_future.GetCallback());
  EXPECT_NE(score_future.Get(), std::nullopt);
}

TEST_F(OnDeviceModelServiceControllerTest, BaseModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      // No weight, so will use base model.
  });
  Initialize(InitializeParams{
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  const std::string expected_response = "Context: execute:foo off:0 max:1024\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_TRUE(*response_.provided_by_on_device());
  EXPECT_THAT(response_.partials(), ElementsAre(expected_response));
  EXPECT_TRUE(response_.log_entry());
  auto logged_on_device_model_execution_info =
      response_.log_entry()
          ->log_ai_data_request()
          ->model_execution_info()
          .on_device_model_execution_info();
  auto model_version = logged_on_device_model_execution_info.model_versions()
                           .on_device_model_service_version();
  EXPECT_EQ(model_version.component_version(), "0.0.1");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_name(),
            "Test");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_version(),
            "0.0.1");
  EXPECT_EQ(model_version.model_adaptation_version(), compose_asset.version());
  EXPECT_GT(logged_on_device_model_execution_info.execution_infos_size(), 0);
  EXPECT_EQ(logged_on_device_model_execution_info.execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);

  EXPECT_TRUE(response_.model_execution_info());
  logged_on_device_model_execution_info =
      response_.model_execution_info()->on_device_model_execution_info();
  model_version = logged_on_device_model_execution_info.model_versions()
                      .on_device_model_service_version();
  EXPECT_EQ(model_version.component_version(), "0.0.1");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_name(),
            "Test");
  EXPECT_EQ(model_version.on_device_base_model_metadata().base_model_version(),
            "0.0.1");
  EXPECT_EQ(model_version.model_adaptation_version(), compose_asset.version());
  EXPECT_GT(logged_on_device_model_execution_info.execution_infos_size(), 0);
  EXPECT_EQ(logged_on_device_model_execution_info.execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_SUCCESS);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kSuccess, 1);

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, AdaptationModelExecutionSuccess) {
  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  Initialize(InitializeParams{
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(),
            "Adaptation model: 1015\nContext: execute:foo off:0 max:1024\n");

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

// Sessions using different adaptations should be able to execute
// concurrently.
TEST_F(OnDeviceModelServiceControllerTest,
       MultipleModelAdaptationExecutionSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kOnDeviceModelTestFeature, {}}}, {});

  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  FakeAdaptationAsset test_asset({
      .config = UnsafeTestConfig(),
      .weight = 2024,
  });
  Initialize(InitializeParams{
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset, &test_asset},
  });

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_compose);
  auto session_test = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_test);

  EXPECT_EQ(2u, GetModelAdaptationControllers().size());

  ResponseHolder compose_response;
  session_compose->ExecuteModel(PageUrlRequest("foo"),
                                compose_response.GetStreamingCallback());
  ResponseHolder test_response;
  session_test->ExecuteModel(PageUrlRequest("bar"),
                             test_response.GetStreamingCallback());

  ASSERT_TRUE(compose_response.GetFinalStatus());
  EXPECT_EQ(*compose_response.value(),
            "Adaptation model: 1015\nContext: execute:foo off:0 max:1024\n");
  EXPECT_TRUE(*compose_response.provided_by_on_device());
  ASSERT_TRUE(test_response.GetFinalStatus());
  EXPECT_EQ(*test_response.value(),
            "Adaptation model: 2024\nContext: execute:bar off:0 max:1024\n");
  EXPECT_TRUE(*test_response.provided_by_on_device());

  session_compose.reset();
  session_test.reset();

  // Fast forward by the amount of time that triggers an idle disconnect. All
  // adaptations and the base model should be reset.
  task_environment_.FastForwardBy(features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

// A session using the base model should be able to execute concurrently
// with one using and adaptation.
TEST_F(OnDeviceModelServiceControllerTest, ModelAdaptationAndBaseModelSuccess) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kOnDeviceModelTestFeature, {}}}, {});

  FakeAdaptationAsset compose_asset({
      .config = SimpleComposeConfig(),
      .weight = 1015,
  });
  FakeAdaptationAsset test_asset({
      .config = UnsafeTestConfig(),
      // no weight, will use base model.
  });
  Initialize(InitializeParams{
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset, &test_asset},
  });

  auto session_compose = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_compose);
  auto session_test = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session_test);

  task_environment_.RunUntilIdle();
  EXPECT_EQ(1u, GetModelAdaptationControllers().size());

  ResponseHolder compose_response;
  session_compose->ExecuteModel(PageUrlRequest("foo"),
                                compose_response.GetStreamingCallback());
  ResponseHolder test_response;
  session_test->ExecuteModel(PageUrlRequest("bar"),
                             test_response.GetStreamingCallback());

  ASSERT_TRUE(compose_response.GetFinalStatus());
  EXPECT_EQ(*compose_response.value(),
            "Adaptation model: 1015\nContext: execute:foo off:0 max:1024\n");
  EXPECT_TRUE(*compose_response.provided_by_on_device());
  ASSERT_TRUE(test_response.GetFinalStatus());
  EXPECT_EQ(*test_response.value(), "Context: execute:bar off:0 max:1024\n");
  EXPECT_TRUE(*test_response.provided_by_on_device());

  session_compose.reset();
  session_test.reset();

  // If we wait long enough, everything should idle out and the service should
  // get terminated. This requires 2 idle timeout intervals (one for the
  // adaptation and one for the base model).
  task_environment_.FastForwardBy(2 * features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest,
       SessionCreationFailsWhenExecutionNotEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kOptimizationGuideComposeOnDeviceEval});

  Initialize(standard_assets_);

  base::HistogramTester histogram_tester;
  auto session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, base::DoNothing(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kFeatureExecutionNotEnabled, 1);
}

// Without a base model available, sessions should fail to be created.
TEST_F(OnDeviceModelServiceControllerTest, BaseModelToBeInstalled) {
  on_device_component_state_manager_.get()->OnStartup();
  task_environment_.FastForwardBy(base::Seconds(1));
  Initialize(InitializeParams{
      .base_model = nullptr,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_FALSE(session);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason.Compose",
      OnDeviceModelEligibilityReason::kModelToBeInstalled, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, BaseModelAvailableAfterInit) {
  on_device_component_state_manager_.get()->OnStartup();
  task_environment_.FastForwardBy(base::Seconds(1));
  Initialize(InitializeParams{
      .base_model = nullptr,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });

  // Model not yet available.
  base::HistogramTester histogram_tester;
  auto session = CreateSession();
  EXPECT_FALSE(session);

  on_device_component_state_manager_.SetReady(
      standard_assets_.base_model.path(),
      standard_assets_.base_model.version());
  task_environment_.RunUntilIdle();

  // Model now available.
  session = CreateSession();
  EXPECT_TRUE(session);
}

// Updating the model should not break existing sessions until a new session
// is started.
TEST_F(OnDeviceModelServiceControllerTest, MidSessionModelUpdate) {
  Initialize(standard_assets_);

  auto session = CreateSession();

  // Simulate a model update.
  FakeBaseModelAsset next_model({
      .weight = 2,
  });
  on_device_component_state_manager_.SetReady(next_model.path(),
                                              next_model.version());
  task_environment_.RunUntilIdle();

  // Verify the existing session still works.
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  // Note that the session does not execute with the new model.
  EXPECT_EQ(*response_.value(), "Context: execute:foo off:0 max:1024\n");
}

TEST_F(OnDeviceModelServiceControllerTest, SessionBeforeAndAfterModelUpdate) {
  Initialize(standard_assets_);

  auto session1 = CreateSession();
  session1->AddContext(UserInputRequest("context"));
  task_environment_.RunUntilIdle();
  EXPECT_EQ(1ull, fake_launcher_.on_device_model_receiver_count());

  // Simulates a model update. This should close the model remote.
  FakeBaseModelAsset next_model({
      .weight = 2,
  });
  on_device_component_state_manager_.SetReady(next_model.path(),
                                              next_model.version());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0ull, fake_launcher_.on_device_model_receiver_count());

  // Create a new session and verify it uses the new model.
  base::HistogramTester histogram_tester;
  auto session2 = CreateSession();
  ASSERT_TRUE(session2);
  ResponseHolder response2;
  session2->ExecuteModel(PageUrlRequest("foo"),
                         response2.GetStreamingCallback());
  ASSERT_TRUE(response2.GetFinalStatus());
  EXPECT_EQ(*response2.value(),
            "Base model: 2\nContext: execute:foo off:0 max:1024\n");
}

TEST_F(OnDeviceModelServiceControllerTest, SessionFailsForInvalidFeature) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::internal::kOnDeviceModelTestFeature);

  Initialize(standard_assets_);
  base::HistogramTester histogram_tester;

  EXPECT_FALSE(test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, base::DoNothing(), logger_.GetWeakPtr(),
      nullptr, /*config_params=*/std::nullopt));

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
      "Test",
      OnDeviceModelEligibilityReason::kConfigNotAvailableForFeature, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, UpdateSafetyModel) {
  FakeSafetyModelAsset fake_safety_asset(ComposeSafetyConfig());

  Initialize(standard_assets_);

  // Safety model info is valid but no metadata.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoMetadata, 1);
  }

  // Safety model info is valid but metadata is of wrong type.
  {
    base::HistogramTester histogram_tester;

    proto::Any any;
    any.set_type_url("garbagetype");
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(any)
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kMetadataWrongType, 1);
  }

  // Safety model info is valid but no feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(AnyWrapProto(model_metadata))
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kNoFeatureConfigs, 1);
  }

  // Safety model info is valid and metadata has feature configs.
  {
    base::HistogramTester histogram_tester;

    proto::TextSafetyModelMetadata model_metadata;
    model_metadata.add_feature_text_safety_configurations()->set_feature(
        ToModelExecutionFeatureProto(kFeature));
    std::unique_ptr<optimization_guide::ModelInfo> model_info =
        TestModelInfoBuilder()
            .SetAdditionalFiles(fake_safety_asset.AdditionalFiles())
            .SetModelMetadata(AnyWrapProto(model_metadata))
            .Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, UpdatingSafetyModelEnablesModels) {
  // Verifies that when we start a session before safety is available, that
  // future session that require a safety model still get one.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {
          {features::internal::kOnDeviceModelTestFeature, {}},
          {features::kTextSafetyClassifier,
           {{"on_device_retract_unsafe_content", "true"}}},
      },
      {});

  FakeAdaptationAsset compose_asset({.config = SimpleComposeConfig()});
  FakeAdaptationAsset test_asset({.config = UnsafeTestConfig()});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = nullptr,
      .language = nullptr,
      .adaptations = {&compose_asset, &test_asset},
  });

  // Compose capability can't start because it's missing safety model.
  EXPECT_FALSE(test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt));

  // Test capability starts because it doesn't require a safety model.
  auto test_session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kTest, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(test_session);

  // Executing with test_session should force model to be loaded.
  ResponseHolder test_response;
  test_session->ExecuteModel(PageUrlRequest("unsafe"),
                             test_response.GetStreamingCallback());
  EXPECT_TRUE(test_response.GetFinalStatus());

  // Compose capability should be available after safety model loads.
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(
        RequireReasonable());
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
  auto compose_session = test_controller_->CreateSession(
      ModelBasedCapabilityKey::kCompose, FailOnRemoteFallback(),
      logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(compose_session);

  ResponseHolder compose_response;
  compose_session->ExecuteModel(PageUrlRequest("unsafe"),
                                compose_response.GetStreamingCallback());

  // Compose should run and be rejected as unsafe.
  EXPECT_FALSE(compose_response.GetFinalStatus());
  EXPECT_EQ(
      compose_response.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
}

TEST_F(OnDeviceModelServiceControllerTest, SessionRequiresSafetyModel) {
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = nullptr,
      .language = nullptr,
      .adaptations = {&standard_assets_.compose},
  });

  // No safety model received yet.
  {
    base::HistogramTester histogram_tester;

    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
  }

  // Safety model info is valid but no config for feature, session not created
  // successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.set_feature(proto::MODEL_EXECUTION_FEATURE_TEST);
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyConfigNotAvailableForFeature, 1);
  }

  // Safety model info is valid, session created successfully.
  {
    base::HistogramTester histogram_tester;

    test_controller_->MaybeUpdateSafetyModel(
        standard_assets_.safety.model_info());
    EXPECT_TRUE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }

  // Safety model reset to not available, session no longer created
  // successfully.
  {
    base::HistogramTester histogram_tester;

    test_controller_->MaybeUpdateSafetyModel(std::nullopt);
    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
    // No model. Shouldn't even record this histogram.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        0);
  }

  // Safety model reset to invalid, session no longer created successfully.
  {
    base::HistogramTester histogram_tester;

    std::unique_ptr<ModelInfo> model_info = TestModelInfoBuilder().Build();
    test_controller_->MaybeUpdateSafetyModel(*model_info);
    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSafetyModelNotAvailable, 1);
    // No required model files. Shouldn't even record this histogram.
    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        0);
  }

  // Safety model info is valid and requires language but no language detection
  // model, session not created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.add_allowed_languages("en");
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kLanguageDetectionModelNotAvailable, 1);
  }

  // Safety model info is valid and requires language, all models available and
  // session created successfully.
  {
    base::HistogramTester histogram_tester;

    FakeSafetyModelAsset safety_asset([]() {
      auto safety_config = ComposeSafetyConfig();
      safety_config.add_allowed_languages("en");
      return safety_config;
    }());
    test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());
    test_controller_->SetLanguageDetectionModel(
        standard_assets_.language.model_info());

    EXPECT_TRUE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution."
        "OnDeviceTextSafetyModelMetadataValidity",
        TextSafetyModelMetadataValidity::kValid, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }

  // No safety model received yet but feature flag should disable safety check.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(features::kTextSafetyClassifier);
    base::HistogramTester histogram_tester;

    test_controller_->MaybeUpdateSafetyModel(std::nullopt);
    EXPECT_TRUE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kSuccess, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, SucceedsWithPassingSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: safe_output")));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRequestSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url")
                          // Raw output check not done.
                          ));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: unsafe_url")
                          // Raw output check not done.
                          ));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackWithInvalidRequestSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", ProtoField({9999})));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(
      fallback_call.logged_executions(),
      ElementsAre(testing::_  // Base Model Execution
                              // Request check failed to run, not logged.
                  ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(OptimizationGuideModelExecutionResult(
               base::ok(fallback.ComposeResponse("remote response")), nullptr),
           nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
  ASSERT_FALSE(response_.model_execution_info());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingRawOutputSafetyChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: unsafe_output")));

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url"),
                          ResultOf("check text", &GetCheckText,
                                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, FallbackWithInvalidRawOutputChecks) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", ProtoField({9999})));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("safe_url"),
                        response_.GetStreamingCallback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(fallback_call.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "request_check: safe_url")
                          // Raw output failed to run, not logged.
                          ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(OptimizationGuideModelExecutionResult(
               base::ok(fallback.ComposeResponse("remote response")), nullptr),
           nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
  EXPECT_FALSE(response_.model_execution_info());
}

TEST_F(OnDeviceModelServiceControllerTest,
       SucceedsWithPassingResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
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
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_very_safe_output")));
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_very_safe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FailsWithFailingResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
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
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_un"),
                        response_.GetStreamingCallback());
  ASSERT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(response_.logged_executions(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_unsafe_output")));
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(),
              ElementsAre(testing::_,  // Base Model Execution
                          ResultOf("check text", &GetCheckText,
                                   "response_check: url_unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackWithInvalidResponseSafetyCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "true"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    {
      auto* check = safety_config.add_response_check();
      auto* i1 = check->add_inputs();
      i1->set_input_type(proto::CHECK_INPUT_TYPE_UNSPECIFIED);
      i1->mutable_templates()->Add(
          FieldSubstitution("response_check: %s", PageUrlField()));
      auto* i2 = check->add_inputs();
      i2->set_input_type(proto::CHECK_INPUT_TYPE_RESPONSE);
      i2->mutable_templates()->Add(FieldSubstitution("%s", ProtoField({1})));
      check->mutable_safety_category_thresholds()->Add(ForbidUnsafe());
      check->set_ignore_language_result(true);
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  ExpectedRemoteFallback fallback;
  auto session = test_controller_->CreateSession(
      kFeature, fallback.CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  fake_settings_.set_execute_result({"safe_output"});
  session->ExecuteModel(PageUrlRequest("url_very_"),
                        response_.GetStreamingCallback());

  auto fallback_call = fallback.Take();
  EXPECT_THAT(
      fallback_call.logged_executions(),
      ElementsAre(testing::_  // Base Model Execution
                              // response check failed to run, not logged.
                  ));
  EXPECT_EQ(fallback_call.feature, ModelBasedCapabilityKey::kCompose);
  std::move(fallback_call.callback)
      .Run(OptimizationGuideModelExecutionResult(
               base::ok(fallback.ComposeResponse("remote response")), nullptr),
           nullptr);

  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(), "remote response");
  EXPECT_FALSE(response_.log_entry());
  EXPECT_FALSE(response_.model_execution_info());
}

TEST_F(OnDeviceModelServiceControllerTest, NoRetractUnsafeContent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kTextSafetyClassifier,
      {{"on_device_retract_unsafe_content", "false"}});

  Initialize(standard_assets_);

  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    {
      auto* check = safety_config.add_request_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("request_check: %s", PageUrlField()));
    }
    {
      auto* check = safety_config.mutable_raw_output_check();
      check->mutable_input_template()->Add(
          FieldSubstitution("raw_output_check: %s", StringValueField()));
    }
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = CreateSession();
  EXPECT_TRUE(session);

  // Should fail the configured checks, but not not be retracted.
  fake_settings_.set_execute_result({"unsafe_output"});
  session->ExecuteModel(PageUrlRequest("unsafe_url"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  // Make sure T&S logged.
  ASSERT_TRUE(response_.log_entry());
  EXPECT_THAT(
      response_.logged_executions(),
      ElementsAre(
          testing::_,  // Base Model Execution
          ResultOf("check text", &GetCheckText, "request_check: unsafe_url"),
          ResultOf("check text", &GetCheckText,  // partial check
                   "raw_output_check: unsafe_output"),
          ResultOf("check text", &GetCheckText,  // complete check
                   "raw_output_check: unsafe_output")));
  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_THAT(
      response_.model_execution_info()
          ->on_device_model_execution_info()
          .execution_infos(),
      ElementsAre(
          testing::_,  // Base Model Execution
          ResultOf("check text", &GetCheckText, "request_check: unsafe_url"),
          ResultOf("check text", &GetCheckText,  // partial check
                   "raw_output_check: unsafe_output"),
          ResultOf("check text", &GetCheckText,  // complete check
                   "raw_output_check: unsafe_output")));
}

TEST_F(OnDeviceModelServiceControllerTest, ReturnsErrorOnServiceDisconnect) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_fallback_to_server_on_disconnect", "false"}});

  Initialize(standard_assets_);

  auto session = CreateSession();
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  fake_launcher_.CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndCancel, 1);

  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnAddContext) {
  Initialize(standard_assets_);
  auto session = CreateSession();
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();

  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  session->AddContext(UserInputRequest("bar"));
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kCancelled, 1);
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  ASSERT_FALSE(response_.log_entry());
  ASSERT_FALSE(response_.model_execution_info());
}

TEST_F(OnDeviceModelServiceControllerTest, CancelsExecuteOnExecute) {
  Initialize(standard_assets_);
  auto session = test_controller_->CreateSession(
      kFeature, FailOnRemoteFallback(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);

  ResponseHolder resp1;
  ResponseHolder resp2;
  session->ExecuteModel(PageUrlRequest("foo"), resp1.GetStreamingCallback());
  session->ExecuteModel(PageUrlRequest("bar"), resp2.GetStreamingCallback());

  EXPECT_FALSE(resp1.GetFinalStatus());
  EXPECT_TRUE(resp2.GetFinalStatus());
  EXPECT_EQ(
      *resp1.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kCancelled);
  EXPECT_EQ(*resp2.value(), "Context: execute:bar off:0 max:1024\n");
}

TEST_F(OnDeviceModelServiceControllerTest, WontStartSessionAfterGpuBlocked) {
  Initialize(standard_assets_);
  // Start a session.
  fake_settings_.service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = CreateSession();
  EXPECT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();

  {
    base::HistogramTester histogram_tester;

    // Because the model returned kGpuBlocked, no more sessions should start.
    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kGpuBlocked, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, DontRecreateSessionIfGpuBlocked) {
  Initialize(standard_assets_);
  fake_settings_.service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = CreateSession();
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  fake_launcher_.clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  session->AddContext(UserInputRequest("baz"));
  EXPECT_FALSE(fake_launcher_.did_launch_service());
}

TEST_F(OnDeviceModelServiceControllerTest, StopsConnectingAfterMultipleDrops) {
  Initialize(standard_assets_);
  // Start a session.
  fake_settings_.set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession()) << i;
    task_environment_.RunUntilIdle();
  }

  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);
}

TEST_F(OnDeviceModelServiceControllerTest, AllowsConnectingAfterBackoffPeriod) {
  Initialize(standard_assets_);
  fake_settings_.set_drop_connection_request(true);

  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession()) << i;
    task_environment_.RunUntilIdle();
  }

  // Immediately starting a session should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward by backoff time and starting a session should succeed.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession());
  task_environment_.RunUntilIdle();

  // Starting another session after another crash should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward base time should not work.
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward again should allow retrying (now 2 * base time).
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ClearsCrashDataOnSuccessAfterBackoff) {
  Initialize(standard_assets_);
  fake_settings_.set_drop_connection_request(true);

  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession()) << i;
    task_environment_.RunUntilIdle();
  }

  // Immediately starting a session should fail.
  ExpectFailedSession(OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Fast forward by backoff time and starting a session should succeed.
  fake_settings_.set_drop_connection_request(false);
  task_environment_.FastForwardBy(
      features::GetOnDeviceModelCrashBackoffBaseTime() + base::Milliseconds(1));
  EXPECT_TRUE(CreateSession());
  task_environment_.RunUntilIdle();

  // Second session should succeed.
  EXPECT_TRUE(CreateSession());

  // Single crash should not disable sessions.
  fake_settings_.set_drop_connection_request(true);
  EXPECT_TRUE(CreateSession());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest, AlternatingDisconnectSucceeds) {
  Initialize(standard_assets_);
  // Start a session.
  for (int i = 0; i < 10; ++i) {
    fake_settings_.set_drop_connection_request(i % 2 == 1);
    EXPECT_TRUE(CreateSession()) << i;
    task_environment_.RunUntilIdle();
  }
}

TEST_F(OnDeviceModelServiceControllerTest,
       MultipleDisconnectsThenVersionChangeRetries) {
  Initialize(standard_assets_);
  // Create enough sessions that fail to trigger no longer creating a session.
  fake_settings_.set_drop_connection_request(true);
  for (int i = 0; i < features::GetOnDeviceModelCrashCountBeforeDisable();
       ++i) {
    EXPECT_TRUE(CreateSession()) << i;
    task_environment_.RunUntilIdle();
  }
  EXPECT_FALSE(CreateSession());
  EXPECT_EQ(test_controller_->CanCreateSession(kFeature),
            OnDeviceModelEligibilityReason::kTooManyRecentCrashes);

  // Change the pref to a different value and recreate the service.
  access_controller_ = nullptr;
  test_controller_.reset();
  pref_service_.SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "BOGUS VERSION");
  RecreateServiceController();
  test_controller_->MaybeUpdateModelAdaptation(
      standard_assets_.compose.feature(), standard_assets_.compose.metadata());
  test_controller_->MaybeUpdateSafetyModel(
      standard_assets_.safety.model_info());
  // Wait until configuration is read.
  task_environment_.RunUntilIdle();

  EXPECT_EQ(test_controller_->CanCreateSession(kFeature),
            OnDeviceModelEligibilityReason::kSuccess);

  // A new session should be started because the version changed.
  EXPECT_TRUE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextDisconnectExecute) {
  Initialize(standard_assets_);
  auto session = CreateSession();
  EXPECT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Launch the service again, which triggers disconnect.
  fake_launcher_.CrashService();
  task_environment_.RunUntilIdle();

  // Send some text, ensuring the context is received.
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("baz"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kUsedOnDevice, 1);
  std::string expected_response =
      ("Context: ctx:foo off:0 max:4096\n"
       "Context: execute:foobaz off:0 max:1024\n");
  EXPECT_EQ(*response_.value(), expected_response);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextExecuteDisconnect) {
  Initialize(standard_assets_);
  auto session = test_controller_->CreateSession(
      kFeature, base::BindRepeating(BadRequestRemote), logger_.GetWeakPtr(),
      nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  session->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();
  // Send the text, this won't make it because the service is immediately
  // killed.
  session->ExecuteModel(PageUrlRequest("bar"),
                        response_.GetStreamingCallback());
  fake_launcher_.CrashService();
  task_environment_.RunUntilIdle();
  ASSERT_FALSE(response_.value());
  ASSERT_FALSE(response_.log_entry());
  ASSERT_FALSE(response_.model_execution_info());
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextMultipleSessions) {
  Initialize(standard_assets_);
  auto session1 = CreateSession();
  EXPECT_TRUE(session1);
  session1->AddContext(UserInputRequest("foo"));
  task_environment_.RunUntilIdle();

  // Start another session.
  auto session2 = CreateSession();
  EXPECT_TRUE(session2);
  session2->AddContext(UserInputRequest("bar"));
  task_environment_.RunUntilIdle();

  session2->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  std::string expected_response1 =
      ("Context: ctx:bar off:0 max:4096\n"
       "Context: execute:bar2 off:0 max:1024\n");
  EXPECT_EQ(*response_.value(), expected_response1);

  ResponseHolder response2;
  session1->ExecuteModel(PageUrlRequest("1"), response2.GetStreamingCallback());
  ASSERT_TRUE(response2.GetFinalStatus());
  std::string expected_response2 =
      ("Context: ctx:foo off:0 max:4096\n"
       "Context: execute:foo1 off:0 max:1024\n");
  EXPECT_EQ(*response2.value(), expected_response2);
}

TEST_F(OnDeviceModelServiceControllerTest, CallsRemoteExecute) {
  Initialize(standard_assets_);
  fake_settings_.service_disconnect_reason =
      on_device_model::ServiceDisconnectReason::kGpuBlocked;
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);

  // Wait for the service to launch, and be shut down.
  task_environment_.RunUntilIdle();
  fake_launcher_.clear_did_launch_service();

  // Adding context should not trigger launching the service again.
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("baz"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kUsingServer, 1);
  }
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  EXPECT_TRUE(remote_execute_called_);
  EXPECT_FALSE(fake_launcher_.did_launch_service());
  // Did not start with on-device, so there should not have been a log entry
  // passed.
  ASSERT_FALSE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest, AddContextInvalidConfig) {
  Initialize({
      .base_model = &standard_assets_.base_model,
  });
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  config.set_feature(ToModelExecutionFeatureProto(kFeature));
  FakeAdaptationAsset bad_compose_asset({.config = config});
  test_controller_->MaybeUpdateModelAdaptation(bad_compose_asset.feature(),
                                               bad_compose_asset.metadata());

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  {
    base::HistogramTester histogram_tester;
    session->AddContext(UserInputRequest("foo"));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceAddContextResult.Compose",
        SessionImpl::AddContextResult::kFailedConstructingInput, 1);
  }
  task_environment_.RunUntilIdle();
  {
    base::HistogramTester histogram_tester;
    session->ExecuteModel(PageUrlRequest("2"),
                          response_.GetStreamingCallback());
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
        ExecuteModelResult::kOnDeviceNotUsed, 1);
  }
  EXPECT_TRUE(remote_execute_called_);
  // The execute call never made it to on-device, so we shouldn't have created a
  // log entry.
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest, ExecuteInvalidConfig) {
  Initialize({
      .base_model = &standard_assets_.base_model,
  });
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_can_skip_text_safety(true);
  config.set_feature(ToModelExecutionFeatureProto(kFeature));
  FakeAdaptationAsset bad_compose_asset({.config = config});
  test_controller_->MaybeUpdateModelAdaptation(bad_compose_asset.feature(),
                                               bad_compose_asset.metadata());

  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  ASSERT_TRUE(session);
  base::HistogramTester histogram_tester;
  session->ExecuteModel(PageUrlRequest("2"), response_.GetStreamingCallback());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kFailedConstructingMessage, 1);
  EXPECT_TRUE(remote_execute_called_);
  EXPECT_FALSE(log_ai_data_request_passed_to_remote_->compose().has_response());
}

TEST_F(OnDeviceModelServiceControllerTest,
       FallbackToServerOnDisconnectWhileWaitingForExecute) {
  Initialize(standard_assets_);
  auto session = test_controller_->CreateSession(
      kFeature, CreateExecuteRemoteFn(), logger_.GetWeakPtr(), nullptr,
      /*config_params=*/std::nullopt);
  EXPECT_TRUE(session);
  task_environment_.RunUntilIdle();
  fake_launcher_.CrashService();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDisconnectAndMaybeFallback, 1);
  EXPECT_TRUE(remote_execute_called_);
  ASSERT_TRUE(log_ai_data_request_passed_to_remote_);
}

TEST_F(OnDeviceModelServiceControllerTest,
       DestroySessionWhileWaitingForResponse) {
  Initialize(standard_assets_);
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  base::HistogramTester histogram_tester;
  const auto total_time = base::Seconds(11);
  task_environment_.AdvanceClock(total_time);
  session.reset();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kDestroyedWhileWaitingForResponse, 1);
  histogram_tester.ExpectUniqueTimeSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceDestroyedWhileWaitingForResponseTime.Compose",
      total_time, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DisconnectsWhenIdle) {
  const base::TimeDelta idle_timeout = base::Seconds(10);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_service_idle_timeout", "10s"}});
  Initialize(standard_assets_);
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  session.reset();
  EXPECT_TRUE(fake_launcher_.is_service_running());

  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  // Should still be connected after half the idle time.
  EXPECT_TRUE(fake_launcher_.is_service_running());

  // Fast forward by the amount of time that triggers a disconnect.
  task_environment_.FastForwardBy(idle_timeout / 2 + base::Milliseconds(1));
  // As there are no sessions and no traffic for GetOnDeviceModelIdleTimeout()
  // the connection should be dropped.
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ShutsDownServiceAfterPerformanceCheck) {
  Initialize(standard_assets_);
  base::test::TestFuture<OnDeviceModelPerformanceClass> result_future;
  OnDeviceModelServiceController::GetEstimatedPerformanceClass(
      test_controller_, result_future.GetCallback());
  EXPECT_EQ(OnDeviceModelPerformanceClass::kVeryHigh, result_future.Get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest,
       PerformanceCheckKeepsControllerAlive) {
  Initialize(standard_assets_);
  auto weak_controller = test_controller_->GetWeakPtr();
  access_controller_ = nullptr;  // Avoid dangling pointer
  base::test::TestFuture<OnDeviceModelPerformanceClass> result_future;
  OnDeviceModelServiceController::GetEstimatedPerformanceClass(
      std::move(test_controller_), result_future.GetCallback());
  EXPECT_EQ(OnDeviceModelPerformanceClass::kVeryHigh, result_future.Get());
  // Verify there wasn't something else keeping the controller alive.
  EXPECT_FALSE(weak_controller);
}

TEST_F(OnDeviceModelServiceControllerTest, RedactedField) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar");
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });
  test_controller_->MaybeUpdateModelAdaptation(compose_asset.feature(),
                                               compose_asset.metadata());

  // `foo` doesn't match the redaction, so should be returned.
  auto session1 = CreateSession();
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("foo"),
                         response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response1 =
      "Context: execute:foo off:0 max:1024\n";
  EXPECT_EQ(*response_.value(), expected_response1);
  EXPECT_THAT(response_.partials(), ElementsAre(expected_response1));

  // Input and output contain text matching redact, so should not be redacted.
  auto session2 = CreateSession();
  ASSERT_TRUE(session2);
  ResponseHolder response2;
  session2->ExecuteModel(UserInputRequest("abarx"),
                         response2.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response2 =
      "Context: execute:abarx off:0 max:1024\n";
  EXPECT_EQ(*response2.value(), expected_response2);
  EXPECT_THAT(response2.partials(), ElementsAre(expected_response2));

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Context: abarx off:0 max:1024\n"});
  auto session3 = CreateSession();
  ASSERT_TRUE(session3);
  ResponseHolder response3;
  session3->ExecuteModel(UserInputRequest("foo"),
                         response3.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response3 = "Context: a[###]x off:0 max:1024\n";
  EXPECT_EQ(*response3.value(), expected_response3);
  EXPECT_THAT(response3.partials(), ElementsAre(expected_response3));
}

TEST_F(OnDeviceModelServiceControllerTest, RejectedField) {
  Initialize(standard_assets_);
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::RedactBehavior::REJECT);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  auto session1 = CreateSession();
  ASSERT_TRUE(session1);
  session1->ExecuteModel(UserInputRequest("bar"),
                         response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(
      *response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);
  // Although we send an error, we should be sending a log entry back so the
  // filtering can be logged.
  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_EQ(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_EQ(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos(0)
                .response()
                .on_device_model_service_response()
                .status(),
            proto::ON_DEVICE_MODEL_SERVICE_RESPONSE_STATUS_RETRACTED);
}

TEST_F(OnDeviceModelServiceControllerTest, UsePreviousResponseForRewrite) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar");
  // Add a rule that identifies `previous_response` of `rewrite_params`.
  auto& output_config = *config.mutable_output_config();
  auto& redact_rules = *output_config.mutable_redact_rules();
  redact_rules.mutable_fields_to_check()->Add(PreviousResponseField());
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  // Force 'bar' to be returned from model.
  fake_settings_.set_execute_result({"Context: bar off:0 max:1024\n"});

  auto session = CreateSession();
  ASSERT_TRUE(session);

  session->ExecuteModel(RewriteRequest("bar"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  // `bar` shouldn't be rewritten as it's in the input.
  const std::string expected_response = "Context: bar off:0 max:1024\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.partials(), ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, ReplacementText) {
  auto config = SimpleComposeConfig();
  config.set_can_skip_text_safety(true);
  *config.mutable_output_config()->mutable_redact_rules() =
      SimpleRedactRule("bar", proto::REDACT_IF_ONLY_IN_OUTPUT, "[redacted]");
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  // Output contains redacted text (and  input doesn't), so redact.
  fake_settings_.set_execute_result({"Context: abarx off:0 max:1024\n"});
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::string expected_response =
      "Context: a[redacted]x off:0 max:1024\n";
  EXPECT_EQ(*response_.value(), expected_response);
  EXPECT_THAT(response_.partials(), ElementsAre(expected_response));
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeats) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
  });
  EXPECT_EQ(*response_.value(),
            expected_responses.back() + " some more repeating text");
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeatsAndCancelsResponse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "true"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " more stuff",
  });
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(response_.value());
  ASSERT_TRUE(response_.error());
  EXPECT_EQ(*response_.error(), OptimizationGuideModelExecutionError::
                                    ModelExecutionError::kResponseLowQuality);

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceExecuteModelResult.Compose",
      ExecuteModelResult::kResponseHadRepeats, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, DetectsRepeatsAcrossResponses) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating",
      " text",
      " some more ",
      "repeating text",
      " more stuff",
  });
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> partial_responses = ConcatResponses({
      "some text",
      " some more repeating",
      " text",
      " some more ",
  });
  EXPECT_EQ(*response_.value(), partial_responses.back() + "repeating text");
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, IgnoresNonRepeatingText) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      features::kOptimizationGuideOnDeviceModel,
      {{"on_device_model_retract_repeats", "false"}});

  base::HistogramTester histogram_tester;
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .adaptations = {&compose_asset},
  });

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  const std::vector<std::string> expected_responses = ConcatResponses({
      "some text",
      " some more repeating text",
      " some more non repeating text",
      " more stuff",
  });
  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_FALSE(response_.log_entry()
                   ->log_ai_data_request()
                   ->model_execution_info()
                   .on_device_model_execution_info()
                   .execution_infos(0)
                   .response()
                   .on_device_model_service_response()
                   .has_repeats());

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_FALSE(response_.model_execution_info()
                   ->on_device_model_execution_info()
                   .execution_infos(0)
                   .response()
                   .on_device_model_service_response()
                   .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      false, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       InitWithNoOnDeviceComponentStateManager) {
  access_controller_ = nullptr;
  test_controller_ = nullptr;

  auto access_controller =
      std::make_unique<OnDeviceModelAccessController>(pref_service_);
  access_controller_ = access_controller.get();
  test_controller_ = base::MakeRefCounted<OnDeviceModelServiceController>(
      std::move(access_controller),
      on_device_component_state_manager_.get()->GetWeakPtr(),
      fake_launcher_.LaunchFn());

  on_device_component_state_manager_.Reset();
  // Init should not crash.
  test_controller_->Init();
}

TEST_F(OnDeviceModelServiceControllerTest, UsesAdapterTopKAndTemperature) {
  auto config = SimpleComposeConfig();
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  auto session = test_controller_->CreateSession(kFeature, base::DoNothing(),
                                                 logger_.GetWeakPtr(), nullptr,
                                                 SessionConfigParams{});
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::vector<std::string> partial_responses = ConcatResponses({
      "Context: execute:foo off:0 max:1024\n",
      "TopK: 4, Temp: 1.5\n",
  });
  EXPECT_EQ(*response_.value(), partial_responses.back());
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, UsesSessionTopKAndTemperature) {
  // Session sampling params should have precedence over feature ones.
  auto config = SimpleComposeConfig();
  config.mutable_sampling_params()->set_top_k(4);
  config.mutable_sampling_params()->set_temperature(1.5);
  FakeAdaptationAsset compose_asset({.config = config});
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });

  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(), nullptr,
      SessionConfigParams{.sampling_params = SamplingParams{
                              .top_k = 3,
                              .temperature = 2,
                          }});
  EXPECT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(response_.value());
  const std::vector<std::string> partial_responses = ConcatResponses({
      "Context: execute:foo off:0 max:1024\n",
      "TopK: 3, Temp: 2\n",
  });
  EXPECT_EQ(*response_.value(), partial_responses.back());
  EXPECT_THAT(response_.partials(), ElementsAreArray(partial_responses));
}

// Validate that a missing partial output config suppresses partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval0) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    return safety_config;
  }());
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2 token3 token4"};
  EXPECT_EQ(*response_.value(), expected_responses.back());
  const std::vector<std::string> partial_expected_responses = {
      "token1", "token1 token2", "token1 token2 token3",
      "token1 token2 token3 token4"};
  EXPECT_THAT(response_.partials(),
              ElementsAreArray(partial_expected_responses));
}

// Validate that token interval 1 evaluates all partial output.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval1) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    return safety_config;
  }());
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(), "token1 token2 token3 token4");
  EXPECT_THAT(response_.partials(), ElementsAreArray({
                                        "token1",
                                        "token1 token2",
                                        "token1 token2 token3",
                                        "token1 token2 token3 token4",
                                    }));
}

// Validate that token interval 3 only evaluates every third and final chunk.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval3) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(3);
    return safety_config;
  }());
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({"token1", " token2", " token3", " token4",
                                     " token5", " token6", " token7"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_EQ(*response_.value(),
            "token1 token2 token3 token4 token5 token6 token7");
  EXPECT_THAT(response_.partials(),
              ElementsAreArray({
                  "token1 token2 token3",
                  "token1 token2 token3 token4 token5 token6",
              }));
}

// Validate that PartialOutputChecks::minimum_tokens is respected.
TEST_F(OnDeviceModelServiceControllerTest, MinimumSafetyTokens) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_minimum_tokens(2);
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    return safety_config;
  }());
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {
      "token1 token2",
      "token1 token2 token3",
      "token1 token2 token3 token4",
  };

  EXPECT_EQ(*response_.value(), expected_responses.back());
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, WaitUntilCompleteToCancel) {
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.set_only_cancel_unsafe_response_on_complete(true);
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.add_allowed_languages("en");
    return safety_config;
  }());
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .language = &standard_assets_.language,
      .adaptations = {&standard_assets_.compose},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"safe", " safe", " lang:en=1.0", " safe", " unsafe"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());

  // The full output was unsafe so it resulted it in it being filtered.
  EXPECT_FALSE(response_.GetFinalStatus());
  EXPECT_EQ(
      response_.error(),
      OptimizationGuideModelExecutionError::ModelExecutionError::kFiltered);

  const std::vector<std::string> expected_responses = {
      // The first two responses are filtered because their language hasn't been
      // detected yet. Because `only_cancel_unsafe_response_on_complete` is
      // true, this doesn't cause the input to be cancelled.
      //
      // "safe", "safe safe",

      // The next two responses are not filtered because the language has been
      // reliably detected as a supported language.
      "safe safe lang:en=1.0", "safe safe lang:en=1.0 safe",

      // The last response is unsafe so it is filtered. Since the output is
      // complete the response is cancelled.
      //
      // "safe safe lang:en=1.0 safe unsafe",
  };
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

// Validate chunk-by-chunk streaming mode works correctly.
TEST_F(OnDeviceModelServiceControllerTest, TsInterval1_ChunkByChunk) {
  // Configure a token interval to enable streaming responses.
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(1);
    return safety_config;
  }());
  FakeAdaptationAsset compose_asset({
      .config =
          []() {
            auto config = SimpleComposeConfig();
            config.mutable_output_config()->set_response_streaming_mode(
                proto::STREAMING_MODE_CHUNK_BY_CHUNK);
            return config;
          }(),
  });
  Initialize({
      .base_model = &standard_assets_.base_model,
      .safety = &safety_asset,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result(
      {"token1", " token2", " token3", " token4"});
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  const std::vector<std::string> expected_responses = {"token1", " token2",
                                                       " token3", " token4"};
  EXPECT_EQ(*response_.value(), "");
  EXPECT_THAT(response_.partials(), ElementsAreArray(expected_responses));
}

TEST_F(OnDeviceModelServiceControllerTest, TestAvailabilityObserver) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::internal::kOnDeviceModelTestFeature, {}}}, {});

  FakeAdaptationAsset test_asset_noweight({.config = UnsafeTestConfig()});
  Initialize({
      .base_model = nullptr,
      .adaptations = {&test_asset_noweight},
  });

  FakeOnDeviceModelAvailabilityObserver availability_observer_compose(
      ModelBasedCapabilityKey::kCompose),
      availability_observer_test(ModelBasedCapabilityKey::kTest);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kCompose, &availability_observer_compose);
  test_controller_->AddOnDeviceModelAvailabilityChangeObserver(
      ModelBasedCapabilityKey::kTest, &availability_observer_test);

  on_device_component_state_manager_.get()->OnStartup();
  task_environment_.RunUntilIdle();
  on_device_component_state_manager_.SetReady(
      standard_assets_.base_model.path(),
      standard_assets_.base_model.version());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);

  FakeAdaptationAsset adaptation_asset({
      .config = UnsafeComposeConfig(),
      .weight = 1015,
  });
  test_controller_->MaybeUpdateModelAdaptation(adaptation_asset.feature(),
                                               adaptation_asset.metadata());
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_test.reason_);
  EXPECT_EQ(OnDeviceModelEligibilityReason::kSuccess,
            availability_observer_compose.reason_);
}

class OnDeviceModelServiceControllerTsIntervalTest
    : public OnDeviceModelServiceControllerTest,
      public ::testing::WithParamInterface<int> {};

TEST_P(OnDeviceModelServiceControllerTsIntervalTest,
       DetectsRepeatsWithSafetyModel) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOptimizationGuideOnDeviceModel,
        {{"on_device_model_retract_repeats", "false"}}},
       {features::kTextSafetyClassifier,
        {{"on_device_retract_unsafe_content", "true"}}}},
      {});

  Initialize(standard_assets_);
  FakeSafetyModelAsset safety_asset([]() {
    auto safety_config = ComposeSafetyConfig();
    safety_config.mutable_safety_category_thresholds()->Add(ForbidUnsafe());
    safety_config.mutable_partial_output_checks()->set_token_interval(
        GetParam());
    return safety_config;
  }());
  test_controller_->MaybeUpdateSafetyModel(safety_asset.model_info());

  auto session = CreateSession();
  EXPECT_TRUE(session);

  fake_settings_.set_execute_result({
      "some text",
      " some more repeating text",
      " some more repeating text",
      " unsafe stuff not processed",
  });
  session->ExecuteModel(UserInputRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(response_.value());
  EXPECT_EQ(*response_.value(),
            "some text some more repeating text some more repeating text");

  ASSERT_TRUE(response_.log_entry());
  EXPECT_GT(response_.log_entry()
                ->log_ai_data_request()
                ->model_execution_info()
                .on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.log_entry()
                  ->log_ai_data_request()
                  ->model_execution_info()
                  .on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());

  ASSERT_TRUE(response_.model_execution_info());
  EXPECT_GT(response_.model_execution_info()
                ->on_device_model_execution_info()
                .execution_infos_size(),
            0);
  EXPECT_TRUE(response_.model_execution_info()
                  ->on_device_model_execution_info()
                  .execution_infos(0)
                  .response()
                  .on_device_model_service_response()
                  .has_repeats());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceResponseHasRepeats.Compose",
      true, 1);
}

INSTANTIATE_TEST_SUITE_P(OnDeviceModelServiceControllerTsIntervalTests,
                         OnDeviceModelServiceControllerTsIntervalTest,
                         testing::ValuesIn<int>({1, 2, 3, 4, 10}));

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationSucceeds) {
  base::HistogramTester histogram_tester;
  FakeBaseModelAsset base_model(WillPassValidationConfig());
  Initialize({.base_model = &base_model});
  task_environment_.RunUntilIdle();
  // Service should be immediately shut down.
  EXPECT_FALSE(fake_launcher_.is_service_running());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationSucceedsImmediatelyWithNoPrompts) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});

  base::HistogramTester histogram_tester;
  FakeBaseModelAsset base_model(proto::OnDeviceModelValidationConfig{});
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  EXPECT_TRUE(CreateSession());

  // Full validation did not need to run.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationBlocksSession) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  FakeBaseModelAsset base_model(WillFailValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kValidationFailed, 1);
  }

  FakeBaseModelAsset next_model(WillPassValidationConfig());
  {
    base::HistogramTester histogram_tester;
    on_device_component_state_manager_.SetReady(next_model.path(),
                                                next_model.version());
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationBlocksSessionPendingCheck) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    EXPECT_FALSE(CreateSession());

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelEligibilityReason."
        "Compose",
        OnDeviceModelEligibilityReason::kValidationPending, 1);
  }

  {
    base::HistogramTester histogram_tester;
    task_environment_.FastForwardBy(base::Seconds(30) + base::Milliseconds(1));
    task_environment_.RunUntilIdle();
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(CreateSession());
}

// TODO(crbug.com/380229867): Flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ModelValidationNewModelVersion \
  DISABLED_ModelValidationNewModelVersion
#else
#define MAYBE_ModelValidationNewModelVersion ModelValidationNewModelVersion
#endif
TEST_F(OnDeviceModelServiceControllerTest,
       MAYBE_ModelValidationNewModelVersion) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});
  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  EXPECT_TRUE(CreateSession());

  FakeBaseModelAsset next_model({
      .weight = 2,
      .config = ExecutionConfigWithValidation(WillFailValidationConfig()),
      .version = "0.0.2",
  });
  {
    base::HistogramTester histogram_tester;
    on_device_component_state_manager_.SetReady(next_model.path(),
                                                next_model.version());
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  EXPECT_FALSE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationNewModelVersionCancelsPreviousValidation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "10s"},
         {"on_device_model_block_on_validation_failure", "true"}}}},
      {});

  base::HistogramTester histogram_tester;

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  // Send a new model update with no validation config.
  FakeBaseModelAsset next_model({
      .weight = 2,
      .version = "0.0.2",
  });
  on_device_component_state_manager_.SetReady(next_model.path(),
                                              next_model.version());
  task_environment_.RunUntilIdle();

  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));

  // Full validation should never run.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution."
      "OnDeviceModelValidationResultOnValidationStarted",
      OnDeviceModelValidationResult::kUnknown, 1);

  EXPECT_TRUE(CreateSession());
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDoesNotRepeat) {
  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationRepeatsOnFailure) {
  FakeBaseModelAsset base_model([]() {
    proto::OnDeviceModelValidationConfig validation_config;
    auto* prompt = validation_config.add_validation_prompts();
    prompt->set_prompt("hello");
    prompt->set_expected_output("goodbye");
    return validation_config;
  }());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    fake_settings_.set_execute_result({"goodbye"});
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kSuccess, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationMaximumRetry) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "0"},
         {"on_device_model_validation_attempt_count", "2"}}}},
      {});
  FakeBaseModelAsset base_model([]() {
    proto::OnDeviceModelValidationConfig validation_config;
    auto* prompt = validation_config.add_validation_prompts();
    prompt->set_prompt("hello");
    prompt->set_expected_output("goodbye");
    return validation_config;
  }());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  {
    base::HistogramTester histogram_tester;
    Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }

  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
  }

  // After a new version, we should re-check.
  pref_service_.SetString(
      model_execution::prefs::localstate::kOnDeviceModelChromeVersion,
      "OLD_VERSION");
  {
    base::HistogramTester histogram_tester;
    RecreateServiceController();
    task_environment_.RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
        OnDeviceModelValidationResult::kNonMatchingOutput, 1);
  }
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kOnDeviceModelValidation);

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationDelayed) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeaturesAndParameters(
      {{features::kOnDeviceModelValidation,
        {{"on_device_model_validation_delay", "30s"}}}},
      {});

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(15) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(15) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       SessionDoesNotInterruptModelValidation) {
  fake_settings_.set_execute_delay(base::Seconds(10));

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  auto session = CreateSession();
  ASSERT_TRUE(session);
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  EXPECT_TRUE(response_.GetFinalStatus());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);

  // Session was created so the service should still be connected.
  EXPECT_TRUE(fake_launcher_.is_service_running());

  // If we destroy all sessions and wait long enough, everything should idle out
  // and the service should get terminated.
  session.reset();
  task_environment_.FastForwardBy(2 * features::GetOnDeviceModelIdleTimeout() +
                                  base::Seconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFails) {
  FakeBaseModelAsset base_model(WillFailValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});
  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kNonMatchingOutput, 1);
}

TEST_F(OnDeviceModelServiceControllerTest, ModelValidationFailsOnCrash) {
  fake_settings_.set_execute_delay(base::Seconds(10));

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  fake_launcher_.CrashService();
  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kServiceCrash, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       PerformanceCheckDoesNotInterruptModelValidation) {
  fake_settings_.set_execute_delay(base::Seconds(10));

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  base::test::TestFuture<OnDeviceModelPerformanceClass> result_future;
  OnDeviceModelServiceController::GetEstimatedPerformanceClass(
      test_controller_, result_future.GetCallback());
  EXPECT_EQ(OnDeviceModelPerformanceClass::kVeryHigh, result_future.Get());
  task_environment_.RunUntilIdle();

  // Performance check sh;ould not shut down service.
  EXPECT_TRUE(fake_launcher_.is_service_running());
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult", 0);

  task_environment_.FastForwardBy(base::Seconds(10) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);
}

TEST_F(OnDeviceModelServiceControllerTest,
       ModelValidationDoesNotInterruptPerformanceCheck) {
  fake_settings_.set_estimated_performance_delay(base::Seconds(10));
  fake_settings_.set_execute_delay(base::Seconds(1));

  FakeBaseModelAsset base_model(WillPassValidationConfig());
  FakeAdaptationAsset compose_asset({.config = UnsafeComposeConfig()});

  base::HistogramTester histogram_tester;
  Initialize({.base_model = &base_model, .adaptations = {&compose_asset}});
  task_environment_.RunUntilIdle();

  base::test::TestFuture<OnDeviceModelPerformanceClass> result_future;
  OnDeviceModelServiceController::GetEstimatedPerformanceClass(
      test_controller_, result_future.GetCallback());

  task_environment_.FastForwardBy(base::Seconds(1) + base::Milliseconds(1));
  task_environment_.RunUntilIdle();
  // Still connected since the performance estimator is running.
  EXPECT_TRUE(fake_launcher_.is_service_running());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecution.OnDeviceModelValidationResult",
      OnDeviceModelValidationResult::kSuccess, 1);

  EXPECT_FALSE(result_future.IsReady());
  EXPECT_EQ(OnDeviceModelPerformanceClass::kVeryHigh, result_future.Get());
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(fake_launcher_.is_service_running());
}

TEST_F(OnDeviceModelServiceControllerTest, LoggingModeAlwaysDisable) {
  TestModelQualityLogsUploaderService test_uploader(&pref_service_);
  Initialize(standard_assets_);
  auto session = test_controller_->CreateSession(
      kFeature, base::DoNothing(), logger_.GetWeakPtr(),
      test_uploader.GetWeakPtr(),
      SessionConfigParams{
          .logging_mode = SessionConfigParams::LoggingMode::kAlwaysDisable});
  ASSERT_TRUE(session);
  ResponseHolder response_holder;
  session->ExecuteModel(UserInputRequest("input"),
                        response_holder.GetStreamingCallback());
  EXPECT_TRUE(response_holder.GetFinalStatus());
  response_holder.ClearLogEntry();
  EXPECT_EQ(0u, test_uploader.uploaded_logs().size());
}

TEST_F(OnDeviceModelServiceControllerTest, SendsPerformanceHint) {
  // Low performance class should use fastest inference.
  pref_service_.SetInteger(
      model_execution::prefs::localstate::kOnDevicePerformanceClass,
      base::to_underlying(OnDeviceModelPerformanceClass::kLow));
  Initialize(standard_assets_);
  auto session = CreateSession();
  session->ExecuteModel(PageUrlRequest("foo"),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(),
            "Fastest inference\nContext: execute:foo off:0 max:1024\n");
}

SkBitmap CreateBlackSkBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  // Setting the pixels to transparent-black.
  memset(bitmap.getPixels(), 0, width * height * 4);
  return bitmap;
}

TEST_F(OnDeviceModelServiceControllerTest, ImageExecutionSuccess) {
  using RequestProto = ::optimization_guide::proto::ExampleForTestingRequest;
  using NestedProto = ::optimization_guide::proto::ExampleForTestingMessage;
  proto::OnDeviceModelExecutionFeatureConfig config;
  config.set_feature(
      ToModelExecutionFeatureProto(ModelBasedCapabilityKey::kCompose));
  auto& input_config = *config.mutable_input_config();
  input_config.set_request_base_name(
      proto::ExampleForTestingRequest().GetTypeName());
  {
    auto& substitution = *input_config.add_input_context_substitutions();
    substitution.set_string_template("%s");
    *substitution.add_substitutions()
         ->add_candidates()
         ->mutable_image_field()
         ->mutable_proto_field() = ProtoField(
        {RequestProto::kNested1FieldNumber, NestedProto::kMediaFieldNumber});
  }
  {
    auto& substitution = *input_config.add_execute_substitutions();
    substitution.set_string_template("%s");
    *substitution.add_substitutions()
         ->add_candidates()
         ->mutable_image_field()
         ->mutable_proto_field() = ProtoField(
        {RequestProto::kNested2FieldNumber, NestedProto::kMediaFieldNumber});
  }
  {
    auto& output_config = *config.mutable_output_config();
    output_config.set_proto_type(proto::ComposeResponse().GetTypeName());
    *output_config.mutable_proto_field() = OutputField();
  }
  FakeAdaptationAsset compose_asset({
      .config = config,
  });
  Initialize(InitializeParams{
      .base_model = &standard_assets_.base_model,
      .safety = &standard_assets_.safety,
      .language = &standard_assets_.language,
      .adaptations = {&compose_asset},
  });
  auto session = CreateSession();
  ASSERT_TRUE(session);
  MultimodalMessage request((proto::ExampleForTestingRequest()));
  request.edit()
      .GetMutableMessage(RequestProto::kNested1FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  request.edit()
      .GetMutableMessage(RequestProto::kNested2FieldNumber)
      .Set(NestedProto::kMediaFieldNumber, CreateBlackSkBitmap(1, 1));
  session->SetInput(std::move(request));
  session->ExecuteModel(proto::ExampleForTestingRequest(),
                        response_.GetStreamingCallback());
  ASSERT_TRUE(response_.GetFinalStatus());
  EXPECT_EQ(*response_.value(),
            "Context: <image> off:0 max:22\nContext: <image> off:0 max:1024\n");
}

}  // namespace optimization_guide
