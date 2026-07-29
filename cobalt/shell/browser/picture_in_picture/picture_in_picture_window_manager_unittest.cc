// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/picture_in_picture/picture_in_picture_window_manager.h"

#include <memory>
#include <optional>

#include "base/test/metrics/histogram_tester.h"
#include "cobalt/shell/browser/shell_test_support.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_video_picture_in_picture_window_controller_impl.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace {

class MockPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  MockPictureInPictureWindowController() = default;
  ~MockPictureInPictureWindowController() override = default;

  MOCK_METHOD(void, Show, (), (override));
  MOCK_METHOD(void, FocusInitiator, (), (override));
  MOCK_METHOD(void, Close, (bool should_pause_video), (override));
  MOCK_METHOD(void, CloseAndFocusInitiator, (), (override));
  MOCK_METHOD(void, OnWindowDestroyed, (bool should_pause_video), (override));
  MOCK_METHOD(content::WebContents*, GetWebContents, (), (override));
  MOCK_METHOD(std::optional<gfx::Rect>, GetWindowBounds, (), (override));
  MOCK_METHOD(content::WebContents*, GetChildWebContents, (), (override));
  MOCK_METHOD(std::optional<url::Origin>, GetOrigin, (), (override));
};

class MyMockVideoPictureInPictureWindowControllerImpl
    : public content::MockVideoPictureInPictureWindowControllerImpl {
 public:
  explicit MyMockVideoPictureInPictureWindowControllerImpl(
      content::WebContents* web_contents)
      : content::MockVideoPictureInPictureWindowControllerImpl(web_contents) {}
  ~MyMockVideoPictureInPictureWindowControllerImpl() override = default;

  MOCK_METHOD(void, Close, (bool), (override));
};

class PictureInPictureWindowManagerTest : public content::ShellTestBase {
 public:
  void SetUp() override {
    content::ShellTestBase::SetUp();

    // Create a test WebContents.
    content::WebContents::CreateParams create_params(browser_context());
    web_contents_ = std::unique_ptr<content::WebContents>(
        content::TestWebContents::Create(create_params));

    // Setup mock controller.
    auto mock_controller =
        std::make_unique<MyMockVideoPictureInPictureWindowControllerImpl>(
            web_contents_.get());
    mock_controller_ = mock_controller.get();
    web_contents_->SetUserData(mock_controller->UserDataKey(),
                               std::move(mock_controller));
  }

  void TearDown() override {
    PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
    mock_controller_ = nullptr;
    web_contents_.reset();
    content::ShellTestBase::TearDown();
  }

 protected:
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<MyMockVideoPictureInPictureWindowControllerImpl> mock_controller_;

  MockPictureInPictureWindowController controller1_;
  MockPictureInPictureWindowController controller2_;
};

TEST_F(PictureInPictureWindowManagerTest, GetWebContentsReturnsNullInitially) {
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
}

TEST_F(PictureInPictureWindowManagerTest,
       EnterAndExitPictureInPictureWithController) {
  content::WebContents* kDummyWebContents =
      reinterpret_cast<content::WebContents*>(0x12345678);
  EXPECT_CALL(controller1_, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));

  base::HistogramTester histogram_tester;

  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller1_);
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            kDummyWebContents);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Enter", true, 1);
  histogram_tester.ExpectTotalCount("Cobalt.PictureInPicture.Exit", 0);

  EXPECT_CALL(controller1_, Close(false));
  PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Exit", true, 1);
}

TEST_F(PictureInPictureWindowManagerTest,
       ReplacingControllerClosesOldController) {
  content::WebContents* kDummyWebContents =
      reinterpret_cast<content::WebContents*>(0x12345678);
  EXPECT_CALL(controller1_, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));
  EXPECT_CALL(controller2_, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));

  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller1_);

  EXPECT_CALL(controller1_, Close(false));
  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller2_);
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            kDummyWebContents);

  EXPECT_CALL(controller2_, Close(false));
  PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
}

TEST_F(PictureInPictureWindowManagerTest,
       EnterWithNullControllerRecordsFailure) {
  base::HistogramTester histogram_tester;

  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(nullptr);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Enter", false, 1);
}

TEST_F(PictureInPictureWindowManagerTest,
       EnterVideoPictureInPictureWithNullWebContentsRecordsFailure) {
  base::HistogramTester histogram_tester;

  PictureInPictureWindowManager::GetInstance().EnterVideoPictureInPicture(
      nullptr);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Enter", false, 1);
}

TEST_F(PictureInPictureWindowManagerTest, WebContentsDestroyedClosesActivePip) {
  PictureInPictureWindowManager::GetInstance().EnterVideoPictureInPicture(
      web_contents_.get());

  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            web_contents_.get());

  // Destroying WebContents should trigger Close on the controller.
  EXPECT_CALL(*mock_controller_, Close(false));
  web_contents_.reset();

  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
}

TEST_F(PictureInPictureWindowManagerTest, NavigationClosesActivePip) {
  PictureInPictureWindowManager::GetInstance().EnterVideoPictureInPicture(
      web_contents_.get());

  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            web_contents_.get());

  // Simulating navigation should trigger Close.
  EXPECT_CALL(*mock_controller_, Close(false));
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("chrome://new-tabs"));

  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
}

}  // namespace
}  // namespace cobalt
