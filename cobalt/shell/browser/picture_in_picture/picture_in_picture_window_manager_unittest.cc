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
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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

class PictureInPictureWindowManagerTest : public testing::Test {
 public:
  void TearDown() override {
    PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
  }
};

TEST_F(PictureInPictureWindowManagerTest, GetWebContentsReturnsNullInitially) {
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
}

TEST_F(PictureInPictureWindowManagerTest,
       EnterAndExitPictureInPictureWithController) {
  content::WebContents* kDummyWebContents =
      reinterpret_cast<content::WebContents*>(0x12345678);
  MockPictureInPictureWindowController controller;
  EXPECT_CALL(controller, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));

  base::HistogramTester histogram_tester;

  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller);
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            kDummyWebContents);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Enter", true, 1);
  histogram_tester.ExpectTotalCount("Cobalt.PictureInPicture.Exit", 0);

  EXPECT_CALL(controller, Close(false));
  PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            nullptr);
  histogram_tester.ExpectBucketCount("Cobalt.PictureInPicture.Exit", true, 1);
}

TEST_F(PictureInPictureWindowManagerTest,
       ReplacingControllerClosesOldController) {
  content::WebContents* kDummyWebContents =
      reinterpret_cast<content::WebContents*>(0x12345678);
  MockPictureInPictureWindowController controller1;
  MockPictureInPictureWindowController controller2;
  EXPECT_CALL(controller1, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));
  EXPECT_CALL(controller2, GetWebContents())
      .WillRepeatedly(testing::Return(kDummyWebContents));

  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller1);

  EXPECT_CALL(controller1, Close(false));
  PictureInPictureWindowManager::GetInstance()
      .EnterPictureInPictureWithController(&controller2);
  EXPECT_EQ(PictureInPictureWindowManager::GetInstance().GetWebContents(),
            kDummyWebContents);

  EXPECT_CALL(controller2, Close(false));
  PictureInPictureWindowManager::GetInstance().ExitPictureInPicture();
}

}  // namespace
}  // namespace cobalt
