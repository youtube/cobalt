// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_checker.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace origin_gating {
namespace {

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               const GURL& source,
               const GURL& destination,
               DoesOriginRequireUserConfirmationCallback callback),
              (const, override));
  MOCK_METHOD(void,
              OnNoVerdict,
              (GatingDecisionContext * context,
               const GURL& source,
               const GURL& destination,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));
};

class OriginGatingCheckerTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<MockDelegate> delegate_;

  void SetUpDelegateExpectations(const GURL& source,
                                 const GURL& destination,
                                 bool requires_user_confirmation,
                                 bool is_allowed,
                                 bool did_prompt_user) {
    EXPECT_CALL(delegate_,
                DoesOriginRequireUserConfirmation(_, source, destination, _))
        .WillOnce(base::test::RunOnceCallback<3>(requires_user_confirmation));

    EXPECT_CALL(delegate_, OnNoVerdict(_, source, destination,
                                       requires_user_confirmation, _))
        .WillOnce(base::test::RunOnceCallback<4>(
            OriginGatingChecker::Delegate::NoVerdictResult{
                .is_allowed = is_allowed, .did_prompt_user = did_prompt_user}));
  }

  GatingDecision ComputeGatingDecisionAndVerifyAsynchrony(
      OriginGatingChecker& checker,
      std::unique_ptr<GatingDecisionContext> context,
      const GURL& source,
      const GURL& destination) {
    base::test::TestFuture<std::unique_ptr<GatingDecisionContext>,
                           GatingDecision>
        future;
    checker.ComputeGatingDecision(std::move(context), source, destination,
                                  future.GetCallback());

    EXPECT_FALSE(future.IsReady());
    return future.Get<1>();
  }
};

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_NoPrompt) {
  OriginGatingChecker checker(delegate_, /*use_site_not_origin=*/false);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.source, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Allowed_WithPrompt) {
  OriginGatingChecker checker(delegate_, /*use_site_not_origin=*/false);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/false,
                            /*is_allowed=*/true,
                            /*did_prompt_user=*/true);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_TRUE(decision.is_allowed);
  EXPECT_EQ(decision.source, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked) {
  OriginGatingChecker checker(delegate_, /*use_site_not_origin=*/false);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/true,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/false);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.source, DecisionSource::kNoVerdict);
}

TEST_F(OriginGatingCheckerTest, FallsBack_Blocked_WithPrompt) {
  OriginGatingChecker checker(delegate_, /*use_site_not_origin=*/false);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  SetUpDelegateExpectations(source, destination,
                            /*requires_user_confirmation=*/true,
                            /*is_allowed=*/false,
                            /*did_prompt_user=*/true);

  GatingDecision decision = ComputeGatingDecisionAndVerifyAsynchrony(
      checker, nullptr, source, destination);

  EXPECT_FALSE(decision.is_allowed);
  EXPECT_EQ(decision.source, DecisionSource::kNoVerdict);
}

class TestGatingContext : public GatingDecisionContext {
 public:
  explicit TestGatingContext(int val) : value(val) {}
  ~TestGatingContext() override = default;
  int value;
};

TEST_F(OriginGatingCheckerTest, PlumbsContext) {
  OriginGatingChecker checker(delegate_, /*use_site_not_origin=*/false);

  GURL source("https://example.com");
  GURL destination("https://foo.com");

  auto context = std::make_unique<TestGatingContext>(42);
  GatingDecisionContext* expected_context_ptr = context.get();

  EXPECT_CALL(delegate_, DoesOriginRequireUserConfirmation(
                             expected_context_ptr, source, destination, _))
      .WillOnce(base::test::RunOnceCallback<3>(true));

  EXPECT_CALL(delegate_, OnNoVerdict(expected_context_ptr, source, destination,
                                     /*requires_user_confirmation=*/true, _))
      .WillOnce(base::test::RunOnceCallback<4>(
          OriginGatingChecker::Delegate::NoVerdictResult{
              .is_allowed = true, .did_prompt_user = false}));

  base::test::TestFuture<std::unique_ptr<GatingDecisionContext>, GatingDecision>
      future;
  checker.ComputeGatingDecision(std::move(context), source, destination,
                                future.GetCallback());

  auto [returned_context, decision] = future.Take();
  ASSERT_TRUE(returned_context);
  EXPECT_EQ(returned_context.get(), expected_context_ptr);
  EXPECT_TRUE(decision.is_allowed);
}

}  // namespace
}  // namespace origin_gating
