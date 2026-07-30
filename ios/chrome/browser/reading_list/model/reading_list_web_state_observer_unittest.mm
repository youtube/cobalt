// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"

#import <memory>

#import "base/memory/scoped_refptr.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/reading_list/model/reading_list_test_utils.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {
const char kTestURL[] = "http://foo.bar";
const char kTestTitle[] = "title";
}  // namespace

// Test fixture to test loading of Reading list entries.
class ReadingListWebStateObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    PlatformTest::SetUp();

    std::vector<scoped_refptr<ReadingListEntry>> initial_entries;
    initial_entries.push_back(base::MakeRefCounted<ReadingListEntry>(
        GURL(kTestURL), kTestTitle, base::Time::Now()));

    TestProfileIOS::Builder builder;
    builder.AddTestingFactory(ReadingListModelFactory::GetInstance(),
                              ReadingListModelTestingFactoryWithFakeStorage(
                                  std::move(initial_entries)));
    profile_ = std::move(builder).Build();

    test_web_state_.SetBrowserState(profile_.get());

    ReadingListWebStateObserver::CreateForWebState(&test_web_state_,
                                                   reading_list_model());
  }

  ReadingListModel* reading_list_model() {
    return ReadingListModelFactory::GetForProfile(profile_.get());
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState test_web_state_;
};

// Tests that failing loading a page does not mark it read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  ASSERT_FALSE(entry->IsRead());

  test_web_state_.SetCurrentURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(entry->IsRead());
}

// Tests that successful loading of a page marks entry read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListSuccess) {
  GURL url(kTestURL);
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model()->GetEntryByURL(url);
  ASSERT_FALSE(entry->IsRead());

  test_web_state_.SetCurrentURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_.SetLoading(false);

  EXPECT_TRUE(entry->IsRead());
}
