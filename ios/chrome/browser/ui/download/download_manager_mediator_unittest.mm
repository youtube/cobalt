// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/download/download_manager_mediator.h"

#import <UIKit/UIKit.h>

#import "base/run_loop.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/download/download_directory_util.h"
#import "ios/chrome/browser/download/external_app_util.h"
#import "ios/chrome/test/fakes/fake_download_manager_consumer.h"
#import "ios/web/public/test/fakes/fake_download_task.h"
#import "ios/web/public/test/web_task_environment.h"
#import "net/base/net_errors.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Constants for configuring a fake download task.
const char kTestUrl[] = "https://chromium.test/download.txt";
const char kTestMimeType[] = "text/html";
const int64_t kTestTotalBytes = 10;
const int64_t kTestReceivedBytes = 0;
const base::FilePath::CharType kTestSuggestedFileName[] =
    FILE_PATH_LITERAL("important_file.zip");

}  // namespace

// Test fixture for testing DownloadManagerMediator class.
class DownloadManagerMediatorTest : public PlatformTest {
 protected:
  DownloadManagerMediatorTest()
      : consumer_([[FakeDownloadManagerConsumer alloc] init]),
        application_(OCMClassMock([UIApplication class])),
        task_(GURL(kTestUrl), kTestMimeType) {
    OCMStub([application_ sharedApplication]).andReturn(application_);
  }
  ~DownloadManagerMediatorTest() override { [application_ stopMocking]; }

  web::FakeDownloadTask* task() { return &task_; }

  DownloadManagerMediator mediator_;
  FakeDownloadManagerConsumer* consumer_;
  id application_;

 private:
  web::WebTaskEnvironment task_environment_;
  web::FakeDownloadTask task_;
};

// Tests starting the download and immediately destroying the task.
// DownloadManagerMediator should not crash.
TEST_F(DownloadManagerMediatorTest, DestoryTaskAfterStart) {
  auto task =
      std::make_unique<web::FakeDownloadTask>(GURL(kTestUrl), kTestMimeType);
  mediator_.SetDownloadTask(task.get());
  mediator_.StartDowloading();
  task.reset();
}

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into Chrome's temporary download
// directory.
TEST_F(DownloadManagerMediatorTest, StartTempDownload) {
  task()->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  // Download file should be located in download directory.
  base::FilePath file = mediator_.GetDownloadPath();
  base::FilePath download_dir;
  ASSERT_TRUE(GetTempDownloadsDirectory(&download_dir));
  EXPECT_TRUE(download_dir.IsParent(file));
}

// Tests starting the download. Verifies that download task is started and its
// file writer is configured to write into Chrome's Documents download
// directory.
TEST_F(DownloadManagerMediatorTest, StartDownload) {
  task()->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  // Download file should be located in download directory.
  base::FilePath download_dir;
  GetTempDownloadsDirectory(&download_dir);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return download_dir.IsParent(mediator_.GetDownloadPath());
      }));

  // Updates the consumer once the file has been moved.
  mediator_.SetDownloadTask(task());
}

// Tests that consumer is updated right after it's set.
TEST_F(DownloadManagerMediatorTest, ConsumerInstantUpdate) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);

  task()->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  task()->SetDone(true);
  task()->SetTotalBytes(kTestTotalBytes);
  task()->SetReceivedBytes(kTestReceivedBytes);
  task()->SetPercentComplete(80);

  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  EXPECT_EQ(base::FilePath(kTestSuggestedFileName),
            base::FilePath(base::SysNSStringToUTF8(consumer_.fileName)));
  EXPECT_EQ(kTestTotalBytes, consumer_.countOfBytesExpectedToReceive);
  EXPECT_EQ(kTestReceivedBytes, consumer_.countOfBytesReceived);
  EXPECT_FLOAT_EQ(0.8f, consumer_.progress);
}

// Tests that consumer changes the state to kDownloadManagerStateFailed if task
// competed with an error.
TEST_F(DownloadManagerMediatorTest, ConsumerFailedStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->SetErrorCode(net::ERR_INTERNET_DISCONNECTED);
  task()->SetState(web::DownloadTask::State::kFailed);
  EXPECT_EQ(kDownloadManagerStateFailed, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateSucceeded if
// task competed without an error.
TEST_F(DownloadManagerMediatorTest, ConsumerSuceededStateUpdate) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(YES);

  task()->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateSucceeded if
// task competed without an error and Google Drive app is not installed.
TEST_F(DownloadManagerMediatorTest,
       ConsumerSuceededStateUpdateWithoutDriveAppInstalled) {
  OCMStub([application_ canOpenURL:GetGoogleDriveAppUrl()]).andReturn(NO);

  task()->SetGeneratedFileName(base::FilePath(kTestSuggestedFileName));
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);
  mediator_.StartDowloading();

  // Starting download is async for task and sync for consumer.
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  ASSERT_TRUE(
      WaitUntilConditionOrTimeout(base::test::ios::kWaitForDownloadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return task()->GetState() == web::DownloadTask::State::kInProgress;
      }));

  task()->SetDone(true);
  EXPECT_EQ(kDownloadManagerStateSucceeded, consumer_.state);
  EXPECT_TRUE(consumer_.installDriveButtonVisible);
}

// Tests that consumer changes the state to kDownloadManagerStateInProgress if
// the task has started.
TEST_F(DownloadManagerMediatorTest, ConsumerInProgressStateUpdate) {
  mediator_.SetDownloadTask(task());
  mediator_.SetConsumer(consumer_);

  task()->Start(base::FilePath());
  EXPECT_EQ(kDownloadManagerStateInProgress, consumer_.state);
  EXPECT_FALSE(consumer_.installDriveButtonVisible);
  EXPECT_EQ(0.0, consumer_.progress);
}
