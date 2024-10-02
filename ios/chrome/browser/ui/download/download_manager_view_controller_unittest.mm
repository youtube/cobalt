// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_view_controller.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/download/download_manager_state_view.h"
#import "ios/chrome/browser/ui/download/radial_progress_view.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for testing DownloadManagerViewController class.
class DownloadManagerViewControllerTest : public PlatformTest {
 protected:
  DownloadManagerViewControllerTest()
      : view_controller_([[DownloadManagerViewController alloc] init]) {
    state_symbol_partial_mock_ = OCMPartialMock(view_controller_.stateSymbol);
  }
  DownloadManagerViewController* view_controller_;
  id state_symbol_partial_mock_;
};

// Tests label and button titles with kDownloadManagerStateNotStarted state
// and long file name.
TEST_F(DownloadManagerViewControllerTest, NotStartedWithLongFileName) {
  view_controller_.state = kDownloadManagerStateNotStarted;
  view_controller_.fileName = @"longfilenamesolongthatitbarelyfitwidthlimit";
  view_controller_.countOfBytesExpectedToReceive = 1024;

  EXPECT_NSEQ(@"longfilenamesolongthatitbarelyfitwidthlimit - 1 KB",
              view_controller_.statusLabel.text);
  EXPECT_NSEQ(@"Download", [view_controller_.actionButton
                               titleForState:UIControlStateNormal]);
  EXPECT_TRUE(view_controller_.progressView.hidden);
}

// Tests label and button titles with kDownloadManagerStateNotStarted state
// and large file size.
TEST_F(DownloadManagerViewControllerTest,
       NotStartedWithLongCountOfExpectedBytes) {
  view_controller_.state = kDownloadManagerStateNotStarted;
  view_controller_.fileName = @"file.zip";
  view_controller_.countOfBytesExpectedToReceive = 1000 * 1024 * 1024;

  EXPECT_NSEQ(@"file.zip - 1.05 GB", view_controller_.statusLabel.text);
  EXPECT_NSEQ(@"Download", [view_controller_.actionButton
                               titleForState:UIControlStateNormal]);
  EXPECT_TRUE(view_controller_.progressView.hidden);
}

// Tests label and button hidden state with kDownloadManagerStateInProgress
// state and long file name.
TEST_F(DownloadManagerViewControllerTest, InProgressWithLongFileName) {
  OCMExpect(
      [state_symbol_partial_mock_ setState:kDownloadManagerStateInProgress]);

  view_controller_.state = kDownloadManagerStateInProgress;
  view_controller_.fileName = @"longfilenamesolongthatitbarelyfitwidthlimit";
  view_controller_.countOfBytesExpectedToReceive = 10 * 1024;
  view_controller_.progress = 0.0f;

  EXPECT_NSEQ(@"Downloading… Zero KB/10 KB", view_controller_.statusLabel.text);
  EXPECT_TRUE(view_controller_.actionButton.hidden);
  EXPECT_OCMOCK_VERIFY(state_symbol_partial_mock_);
  EXPECT_FALSE(view_controller_.progressView.hidden);
  EXPECT_EQ(0.0f, view_controller_.progressView.progress);
}

// Tests label and button hidden state with kDownloadManagerStateInProgress
// state and unknown download size.
TEST_F(DownloadManagerViewControllerTest,
       InProgressWithUnknownCountOfExpectedBytes) {
  OCMExpect(
      [state_symbol_partial_mock_ setState:kDownloadManagerStateInProgress]);

  view_controller_.state = kDownloadManagerStateInProgress;
  view_controller_.fileName = @"file.zip";
  view_controller_.countOfBytesReceived = 900;
  view_controller_.countOfBytesExpectedToReceive = -1;
  view_controller_.progress = 0.9f;

  EXPECT_NSEQ(@"Downloading… 900 bytes", view_controller_.statusLabel.text);
  EXPECT_TRUE(view_controller_.actionButton.hidden);
  EXPECT_OCMOCK_VERIFY(state_symbol_partial_mock_);
  EXPECT_FALSE(view_controller_.progressView.hidden);
  EXPECT_EQ(0.9f, view_controller_.progressView.progress);
}

// Tests label and button titles with kDownloadManagerStateSucceeded state.
TEST_F(DownloadManagerViewControllerTest, SuceededWithWithLongFileName) {
  OCMExpect(
      [state_symbol_partial_mock_ setState:kDownloadManagerStateSucceeded]);

  view_controller_.state = kDownloadManagerStateSucceeded;
  view_controller_.fileName = @"file.txt";
  view_controller_.countOfBytesReceived = 1024;

  EXPECT_NSEQ(@"file.txt", view_controller_.statusLabel.text);
  EXPECT_NSEQ(@"Open in…", [view_controller_.actionButton
                               titleForState:UIControlStateNormal]);
  EXPECT_OCMOCK_VERIFY(state_symbol_partial_mock_);
  EXPECT_TRUE(view_controller_.progressView.hidden);
}

// Tests label and button titles with kDownloadManagerStateFailed state.
TEST_F(DownloadManagerViewControllerTest, Failed) {
  OCMExpect([state_symbol_partial_mock_ setState:kDownloadManagerStateFailed]);

  view_controller_.state = kDownloadManagerStateFailed;
  view_controller_.fileName = @"file.txt";
  view_controller_.countOfBytesReceived = 1024;

  EXPECT_NSEQ(@"Couldn't Download", view_controller_.statusLabel.text);
  EXPECT_NSEQ(@"Try Again", [view_controller_.actionButton
                                titleForState:UIControlStateNormal]);
  EXPECT_OCMOCK_VERIFY(state_symbol_partial_mock_);
  EXPECT_TRUE(view_controller_.progressView.hidden);
}

// Tests that tapping close button calls downloadManagerViewControllerDidClose:.
TEST_F(DownloadManagerViewControllerTest, Close) {
  id delegate =
      OCMStrictProtocolMock(@protocol(DownloadManagerViewControllerDelegate));
  OCMExpect([delegate downloadManagerViewControllerDidClose:view_controller_]);

  view_controller_.state = kDownloadManagerStateNotStarted;
  view_controller_.delegate = delegate;
  [view_controller_.closeButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that tapping Download button calls
// downloadManagerViewControllerDidStartDownload:.
TEST_F(DownloadManagerViewControllerTest, Start) {
  id delegate =
      OCMStrictProtocolMock(@protocol(DownloadManagerViewControllerDelegate));
  OCMExpect([delegate
      downloadManagerViewControllerDidStartDownload:view_controller_]);

  view_controller_.state = kDownloadManagerStateNotStarted;
  view_controller_.delegate = delegate;
  [view_controller_.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that tapping Open In... button calls
// presentOpenInForDownloadManagerViewController:.
TEST_F(DownloadManagerViewControllerTest, OpenIn) {
  id delegate =
      OCMStrictProtocolMock(@protocol(DownloadManagerViewControllerDelegate));
  OCMExpect([delegate
      presentOpenInForDownloadManagerViewController:view_controller_]);

  view_controller_.state = kDownloadManagerStateSucceeded;
  view_controller_.delegate = delegate;
  [view_controller_.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests that tapping Restart button calls
// downloadManagerViewControllerDidStartDownload:.
TEST_F(DownloadManagerViewControllerTest, Restart) {
  id delegate =
      OCMStrictProtocolMock(@protocol(DownloadManagerViewControllerDelegate));
  OCMExpect([delegate
      downloadManagerViewControllerDidStartDownload:view_controller_]);

  view_controller_.state = kDownloadManagerStateFailed;
  view_controller_.delegate = delegate;
  [view_controller_.actionButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];

  EXPECT_OCMOCK_VERIFY(delegate);
}

// Tests making Install Google drive button visible and hidden.
TEST_F(DownloadManagerViewControllerTest, InstallDriveButton) {
  // The button itself is not hidden, but the superview which contains the
  // button is transparent.
  ASSERT_EQ(0.0f, view_controller_.installDriveButton.superview.alpha);

  [view_controller_ setInstallDriveButtonVisible:YES animated:NO];
  // Superview which contains the button is now opaque.
  EXPECT_EQ(1.0f, view_controller_.installDriveButton.superview.alpha);

  [view_controller_ setInstallDriveButtonVisible:NO animated:NO];
  // Superview which contains the button is transparent again.
  EXPECT_EQ(0.0f, view_controller_.installDriveButton.superview.alpha);
}
