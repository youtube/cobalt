// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"

#import <memory>

#import "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/single_thread_task_runner.h"
#import "base/test/simple_test_clock.h"
#import "components/favicon/core/large_icon_service_impl.h"
#import "components/favicon/core/test/mock_favicon_service.h"
#import "components/favicon_base/favicon_types.h"
#import "components/reading_list/core/fake_reading_list_model_storage.h"
#import "components/reading_list/core/reading_list_model_impl.h"
#import "components/sync/base/storage_type.h"
#import "components/url_formatter/url_formatter.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_accessibility_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_custom_action_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_item.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;

namespace reading_list {

// ReadingListMediatorTest is parameterized on this enum to test both
// FaviconAttributesProvider and FaviconLoader.
// TODO(crbug.com/878796): Remove as part of UIRefresh cleanup.
enum class FaviconServiceType {
  FAVICON_LOADER,
  ATTRIBUTES_PROVIDER,
};

class ReadingListMediatorTest
    : public PlatformTest,
      public ::testing::WithParamInterface<FaviconServiceType> {
 public:
  ReadingListMediatorTest() {
    auto storage = std::make_unique<FakeReadingListModelStorage>();
    base::WeakPtr<FakeReadingListModelStorage> storage_ptr =
        storage->AsWeakPtr();
    model_ = std::make_unique<ReadingListModelImpl>(
        std::move(storage), syncer::StorageType::kUnspecified, &clock_);
    // Complete the initial model load from storage.
    storage_ptr->TriggerLoadCompletion();

    EXPECT_CALL(mock_favicon_service_,
                GetLargestRawFaviconForPageURL(_, _, _, _, _))
        .WillRepeatedly([](auto, auto, auto,
                           favicon_base::FaviconRawBitmapCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::SingleThreadTaskRunner::GetCurrentDefault().get(),
              FROM_HERE,
              base::BindOnce(std::move(callback),
                             favicon_base::FaviconRawBitmapResult()));
        });

    no_title_entry_url_ = GURL("http://chromium.org/unread3");
    // The first 3 have the same update time on purpose.
    model_->AddOrReplaceEntry(GURL("http://chromium.org/unread1"), "unread1",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/base::TimeDelta());
    model_->AddOrReplaceEntry(GURL("http://chromium.org/read1"), "read1",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/base::TimeDelta());
    model_->SetReadStatusIfExists(GURL("http://chromium.org/read1"), true);
    model_->AddOrReplaceEntry(GURL("http://chromium.org/unread2"), "unread2",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/base::TimeDelta());
    clock_.Advance(base::Milliseconds(10));
    model_->AddOrReplaceEntry(no_title_entry_url_, "",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/base::TimeDelta());
    clock_.Advance(base::Milliseconds(10));
    model_->AddOrReplaceEntry(GURL("http://chromium.org/read2"), "read2",
                              reading_list::ADDED_VIA_CURRENT_APP,
                              /*estimated_read_time=*/base::TimeDelta());
    model_->SetReadStatusIfExists(GURL("http://chromium.org/read2"), true);
    large_icon_service_.reset(new favicon::LargeIconServiceImpl(
        &mock_favicon_service_, /*image_fetcher=*/nullptr,
        /*desired_size_in_dip_for_server_requests=*/24,
        /*icon_type_for_server_requests=*/
        favicon_base::IconType::kTouchIcon,
        /*google_server_client_param=*/"test_chrome"));

    favicon_loader.reset(new FaviconLoader(large_icon_service_.get()));
    mediator_ = [[ReadingListMediator alloc]
          initWithModel:model_.get()
          faviconLoader:favicon_loader.get()
        listItemFactory:[[ReadingListListItemFactory alloc] init]];
  }

  ReadingListMediatorTest(const ReadingListMediatorTest&) = delete;
  ReadingListMediatorTest& operator=(const ReadingListMediatorTest&) = delete;

 protected:
  testing::StrictMock<favicon::MockFaviconService> mock_favicon_service_;
  std::unique_ptr<ReadingListModelImpl> model_;
  ReadingListMediator* mediator_;
  base::SimpleTestClock clock_;
  GURL no_title_entry_url_;
  std::unique_ptr<FaviconLoader> favicon_loader;
  std::unique_ptr<favicon::LargeIconServiceImpl> large_icon_service_;

 private:
  web::WebTaskEnvironment task_environment_;
};

TEST_P(ReadingListMediatorTest, fillItems) {
  // Setup.
  NSMutableArray<id<ReadingListListItem>>* readArray = [NSMutableArray array];
  NSMutableArray<id<ReadingListListItem>>* unreadArray = [NSMutableArray array];

  // Action.
  [mediator_ fillReadItems:readArray unreadItems:unreadArray];

  // Tests.
  EXPECT_EQ(3U, [unreadArray count]);
  EXPECT_EQ(2U, [readArray count]);
  NSArray<ReadingListTableViewItem*>* rlReadArray = [readArray copy];
  NSArray<ReadingListTableViewItem*>* rlUneadArray = [unreadArray copy];
  EXPECT_TRUE([rlUneadArray[0].title isEqualToString:@""]);
  EXPECT_TRUE([rlReadArray[0].title isEqualToString:@"read2"]);
  EXPECT_TRUE([rlReadArray[1].title isEqualToString:@"read1"]);
}

INSTANTIATE_TEST_SUITE_P(
    ,  // Empty instatiation name.
    ReadingListMediatorTest,
    ::testing::Values(FaviconServiceType::FAVICON_LOADER,
                      FaviconServiceType::ATTRIBUTES_PROVIDER));

}  // namespace reading_list
