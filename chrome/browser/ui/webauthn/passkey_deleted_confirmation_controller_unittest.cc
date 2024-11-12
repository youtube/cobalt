// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webauthn/passkey_deleted_confirmation_controller.h"

#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::Return;

class PasskeyDeletedConfirmationControllerTest : public ::testing::Test {
 public:
  ~PasskeyDeletedConfirmationControllerTest() override = default;

  void SetUp() override {
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  PasskeyDeletedConfirmationController* controller() {
    return controller_.get();
  }

  void CreateController() {
    EXPECT_CALL(*delegate(), OnBubbleShown());
    controller_ = std::make_unique<PasskeyDeletedConfirmationController>(
        mock_delegate_->AsWeakPtr(),
        password_manager::metrics_util::AUTOMATIC_PASSKEY_DELETED_CONFIRMATION);
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  }

  void DestroyController() { controller_.reset(); }

 private:
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<PasskeyDeletedConfirmationController> controller_;
};

TEST_F(PasskeyDeletedConfirmationControllerTest, Destroy) {
  CreateController();
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller()->OnBubbleClosing();
}

TEST_F(PasskeyDeletedConfirmationControllerTest, DestroyImplicictly) {
  CreateController();
  EXPECT_CALL(*delegate(), OnBubbleHidden());
}

}  // namespace
