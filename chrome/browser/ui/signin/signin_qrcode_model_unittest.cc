// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/signin_qrcode_model.h"

#include <memory>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockObserver : public SigninQRCodeModel::Observer {
 public:
  MockObserver() = default;
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnQrCodeChanged, (std::string_view), (override));
  MOCK_METHOD(void, OnQrCodeReset, (), (override));
  MOCK_METHOD(void, OnModelDestroyed, (SigninQRCodeModel*), (override));
};

class SigninQRCodeModelTest : public ChromeRenderViewHostTestHarness {
 public:
  SigninQRCodeModelTest() = default;
  ~SigninQRCodeModelTest() override = default;
};

TEST_F(SigninQRCodeModelTest, SetAndResetQrCode) {
  SigninQRCodeModel* model =
      SigninQRCodeModel::GetOrCreateForWebContents(web_contents());
  ASSERT_TRUE(model);

  testing::StrictMock<MockObserver> observer;
  base::ScopedObservation<SigninQRCodeModel, SigninQRCodeModel::Observer>
      observation(&observer);
  observation.Observe(model);

  // Setting the QR code should notify the observer.
  const std::string qr_code = "test_qr_code_payload";
  EXPECT_CALL(observer, OnQrCodeChanged(std::string_view(qr_code))).Times(1);
  model->SetQrCode(qr_code);
  EXPECT_EQ(model->qr_code_string(), std::optional<std::string_view>(qr_code));

  // Resetting the QR code should notify the observer and clear the cache.
  EXPECT_CALL(observer, OnQrCodeReset()).Times(1);
  model->Reset();
  EXPECT_FALSE(model->qr_code_string().has_value());
}

TEST_F(SigninQRCodeModelTest, DestructionNotification) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  SigninQRCodeModel* model =
      SigninQRCodeModel::GetOrCreateForWebContents(contents.get());
  ASSERT_TRUE(model);

  testing::StrictMock<MockObserver> observer;
  base::ScopedObservation<SigninQRCodeModel, SigninQRCodeModel::Observer>
      observation(&observer);
  observation.Observe(model);

  // Destroying the WebContents should destroy the model and notify observers.
  EXPECT_CALL(observer, OnModelDestroyed(model))
      .WillOnce([&observation](SigninQRCodeModel* m) { observation.Reset(); });
  contents.reset();
}

}  // namespace
