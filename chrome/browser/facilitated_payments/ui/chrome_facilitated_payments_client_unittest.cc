// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/mock_optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

class MockFacilitatedPaymentsController : public FacilitatedPaymentsController {
 public:
  explicit MockFacilitatedPaymentsController(content::WebContents* web_contents)
      : FacilitatedPaymentsController(web_contents) {}
  ~MockFacilitatedPaymentsController() override = default;

  MOCK_METHOD(bool, IsInLandscapeMode, (), (override));
  MOCK_METHOD(
      void,
      Show,
      (base::span<const autofill::BankAccount> bank_account_suggestions,
       base::OnceCallback<void(bool, int64_t)> on_user_decision_callback),
      (override));
  MOCK_METHOD(
      void,
      ShowForEwallet,
      (base::span<const autofill::Ewallet> ewallet_suggestions,
       base::OnceCallback<void(bool, int64_t)> on_user_decision_callback),
      (override));
  MOCK_METHOD(void, ShowProgressScreen, (), (override));
  MOCK_METHOD(void, ShowErrorScreen, (), (override));
  MOCK_METHOD(void, Dismiss, (), (override));
};

class ChromeFacilitatedPaymentsClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    client_ = std::make_unique<ChromeFacilitatedPaymentsClient>(
        web_contents(), &optimization_guide_decider_);
    auto controller =
        std::make_unique<MockFacilitatedPaymentsController>(web_contents());
    controller_ = controller.get();
    client().SetFacilitatedPaymentsControllerForTesting(std::move(controller));
  }

  void TearDown() override {
    controller_ = nullptr;
    client_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  auto& base_client() {
    return static_cast<payments::facilitated::FacilitatedPaymentsClient&>(
        client());
  }
  ChromeFacilitatedPaymentsClient& client() { return *client_; }
  MockFacilitatedPaymentsController& controller() { return *controller_; }

 private:
  optimization_guide::MockOptimizationGuideDecider optimization_guide_decider_;
  std::unique_ptr<ChromeFacilitatedPaymentsClient> client_;
  raw_ptr<MockFacilitatedPaymentsController> controller_;
};

TEST_F(ChromeFacilitatedPaymentsClientTest, GetPaymentsDataManager) {
  EXPECT_NE(nullptr, base_client().GetPaymentsDataManager());
}

TEST_F(ChromeFacilitatedPaymentsClientTest,
       GetFacilitatedPaymentsNetworkInterface) {
  EXPECT_NE(nullptr, base_client().GetFacilitatedPaymentsNetworkInterface());
}

// Test the client forwards call to show Pix FOP selector to the controller.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowPixPaymentPrompt_ControllerDefaultTrue) {
  EXPECT_CALL(controller(), Show);

  base_client().ShowPixPaymentPrompt({}, base::DoNothing());
}

// Test the client forwards call for showing the progress screen to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, ShowProgressScreen) {
  EXPECT_CALL(controller(), ShowProgressScreen);

  base_client().ShowProgressScreen();
}

// Test the client forwards call for showing the error screen to the controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, ShowErrorScreen) {
  EXPECT_CALL(controller(), ShowErrorScreen);

  base_client().ShowErrorScreen();
}

// Test that the controller is able to process requests to show different
// screens back to back.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ControllerIsAbleToProcessBackToBackShowRequests) {
  EXPECT_CALL(controller(), Show);
  EXPECT_CALL(controller(), ShowProgressScreen);

  base_client().ShowPixPaymentPrompt({}, base::DoNothing());
  base_client().ShowProgressScreen();
}

// Test the client forwards call for closing the bottom sheet to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, DismissPrompt) {
  EXPECT_CALL(controller(), Dismiss);

  base_client().DismissPrompt();
}

// Test the client forwards call to check the device screen orientation to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest, IsInLandscapeMode) {
  EXPECT_CALL(controller(), IsInLandscapeMode);

  base_client().IsInLandscapeMode();
}

// Test that the client forwards call to show eWallet FOP selector to the
// controller.
TEST_F(ChromeFacilitatedPaymentsClientTest,
       ShowEwalletPaymentPrompt_ControllerInvoked) {
  EXPECT_CALL(controller(), ShowForEwallet);
  base_client().ShowEwalletPaymentPrompt({}, base::DoNothing());
}
