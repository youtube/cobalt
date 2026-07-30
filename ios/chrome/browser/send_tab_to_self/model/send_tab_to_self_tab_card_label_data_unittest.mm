// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_tab_card_label_data.h"

#import "base/test/task_environment.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

class SendTabToSelfTabCardLabelDataTest : public PlatformTest {
 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the label data is successfully attached and is cleared
// automatically when the tab is shown.
TEST_F(SendTabToSelfTabCardLabelDataTest, WasShownClearsLabel) {
  web::FakeWebState web_state;

  // No label data should be attached initially.
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "remote_device");

  SendTabToSelfTabCardLabelData* label_data =
      SendTabToSelfTabCardLabelData::FromWebState(&web_state);
  ASSERT_NE(nullptr, label_data);
  EXPECT_NSEQ(@"From remote_device", label_data->GetLabelText());

  // Simulating viewing the tab should clear the label.
  web_state.WasShown();
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
}

// Tests that the label data is successfully cleaned up when the WebState is
// destroyed without ever being shown.
TEST_F(SendTabToSelfTabCardLabelDataTest, WebStateDestroyedClearsLabel) {
  auto web_state = std::make_unique<web::FakeWebState>();

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(web_state.get(),
                                                   "remote_device");

  // Verify it is attached.
  EXPECT_NE(nullptr,
            SendTabToSelfTabCardLabelData::FromWebState(web_state.get()));

  // Destroy the WebState. This will trigger WebStateDestroyed, removing the
  // observer and safely destructing the label data.
  web_state.reset();
}

// Tests that the label data automatically expires after 5 days.
TEST_F(SendTabToSelfTabCardLabelDataTest, ExpiryClearsLabel) {
  web::FakeWebState web_state;

  // Attach the label.
  SendTabToSelfTabCardLabelData::CreateForWebState(&web_state, "remote_device");

  SendTabToSelfTabCardLabelData* label_data =
      SendTabToSelfTabCardLabelData::FromWebState(&web_state);
  ASSERT_NE(nullptr, label_data);

  // Fast forward 4 days (less than 5 days). The label should still be valid.
  task_environment_.FastForwardBy(base::Days(4));
  EXPECT_NE(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));

  // Fast forward another 2 days (total 6 days). The label should now be
  // expired and cleared.
  task_environment_.FastForwardBy(base::Days(2));
  EXPECT_EQ(nullptr, SendTabToSelfTabCardLabelData::FromWebState(&web_state));
}
