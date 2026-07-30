// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/model/tool_controller.h"

#import <optional>

#import "base/run_loop.h"
#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_factory.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/actor_tool_request.h"
#import "ios/chrome/browser/intelligence/actor/tools/model/tool_delegate.h"
#import "ios/chrome/browser/intelligence/actor/util/actor_test_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace actor {
namespace {

// A test tool that never completes, added to test ToolController::Cancel.
class AsyncActorTool : public ActorTool {
 public:
  void Execute(ToolExecutionCallback callback) override {
    // Do not run the callback, simulating an async operation that gets
    // cancelled.
  }
  base::WeakPtr<web::WebState> GetTargetWebState() const override {
    return nullptr;
  }
  ToolType GetToolType() const override { return ToolType::kWait; }
};

class AsyncActorToolFactory : public ActorToolFactory {
 public:
  explicit AsyncActorToolFactory(ProfileIOS* profile)
      : ActorToolFactory(profile) {}
  base::expected<std::unique_ptr<ActorTool>, ToolExecutionResult> CreateTool(
      const optimization_guide::proto::Action& action,
      ToolDelegate* tool_delegate) override {
    return std::make_unique<AsyncActorTool>();
  }
};

class ToolControllerTest : public PlatformTest, public ToolDelegate {
 protected:
  ToolControllerTest() {
    profile_ = TestProfileIOS::Builder().Build();
    journal_ = std::make_unique<AggregatedJournal>();
    tool_factory_ = std::make_unique<ActorToolFactory>(profile_.get());
  }

  void SetUp() override {
    PlatformTest::SetUp();
    controller_ = std::make_unique<ToolController>(this);
  }

  // ToolDelegate overrides.
  ActorTaskId GetTaskId() const override { return ActorTaskId(1); }
  AggregatedJournal& GetJournal() const override { return *journal_; }
  ActorToolFactory& GetToolFactory() const override { return *tool_factory_; }
  actor_login::ActorLoginService* GetActorLoginService() override {
    return nullptr;
  }
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      CredentialSelectedCallback callback) override {}
  std::optional<CredentialWithPermission> GetUserSelectedCredential(
      const url::Origin& request_origin) const override {
    return std::nullopt;
  }
  void InterruptFromTool() override {}
  void UninterruptFromTool() override {}

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<ActorToolFactory> tool_factory_;
  std::unique_ptr<AggregatedJournal> journal_;
  std::unique_ptr<ToolController> controller_;
};

// Tests that a tool can be created, validated, and invoked successfully.
TEST_F(ToolControllerTest, SuccessfulExecutionFlow) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  base::RunLoop run_loop;
  bool callback_called = false;
  std::optional<ToolExecutionResult> validation_result;

  controller_->CreateToolAndValidate(
      *request, base::BindOnce(
                    [](bool* called, std::optional<ToolExecutionResult>* out,
                       base::RunLoop* loop, ToolExecutionResult result) {
                      *called = true;
                      *out = result;
                      loop->Quit();
                    },
                    &callback_called, &validation_result, &run_loop));

  run_loop.Run();
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(validation_result.has_value());
  EXPECT_TRUE(validation_result->IsOk());

  base::RunLoop run_loop2;
  bool invoke_callback_called = false;
  std::optional<ToolExecutionResult> invoke_result;

  controller_->Invoke(base::BindOnce(
      [](bool* called, std::optional<ToolExecutionResult>* out,
         base::RunLoop* loop, ToolExecutionResult result) {
        *called = true;
        *out = result;
        loop->Quit();
      },
      &invoke_callback_called, &invoke_result, &run_loop2));

  run_loop2.Run();
  EXPECT_TRUE(invoke_callback_called);
  ASSERT_TRUE(invoke_result.has_value());
  EXPECT_TRUE(invoke_result->IsOk());
}

// Tests that if tool creation fails synchronously, it transitions back to READY
// and doesn't crash the state machine during Cancel.
TEST_F(ToolControllerTest, SyncCreationFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  // An invalid request that will fail tool creation
  optimization_guide::proto::Action action;
  // action has action_case == ACTION_NOT_SET, which will fail creation
  ActorToolRequest request(action);

  base::RunLoop run_loop;
  bool callback_called = false;
  std::optional<ToolExecutionResult> creation_result;

  controller_->CreateToolAndValidate(
      request, base::BindOnce(
                   [](bool* called, std::optional<ToolExecutionResult>* out,
                      base::RunLoop* loop, ToolExecutionResult result) {
                     *called = true;
                     *out = result;
                     loop->Quit();
                   },
                   &callback_called, &creation_result, &run_loop));

  run_loop.Run();
  EXPECT_TRUE(callback_called);
  ASSERT_TRUE(creation_result.has_value());
  EXPECT_FALSE(creation_result->IsOk());

  // Cancel should be a no-op / safe transition.
  controller_->Cancel();
}

// Tests that a tool can be canceled during execution.
TEST_F(ToolControllerTest, CancelMidExecution) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kActorTools);

  tool_factory_ = std::make_unique<AsyncActorToolFactory>(profile_.get());

  std::unique_ptr<ActorToolRequest> request = MakeSuccessfulActorToolRequest();

  controller_->CreateToolAndValidate(
      *request, base::BindOnce([](ToolExecutionResult result) {}));

  controller_->Invoke(base::BindOnce([](ToolExecutionResult result) {
    FAIL() << "Callback should not be called when cancelled.";
  }));

  controller_->Cancel();
}

}  // namespace
}  // namespace actor
