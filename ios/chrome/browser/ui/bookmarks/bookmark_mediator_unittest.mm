// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/url_and_title.h"
#import "components/bookmarks/common/storage_type.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/test/test_sync_service.h"
#import "ios/chrome/browser/bookmarks/model/bookmark_ios_unit_test_support.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/fake_system_identity.h"
#import "ios/chrome/browser/signin/fake_system_identity_manager.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util.h"

@interface BookmarkMediator ()
- (NSString*)messageForAddingBookmarksInFolder:(BOOL)addFolder
                             folderStorageType:
                                 (bookmarks::StorageType)storageType
                                         title:(NSString*)folderTitle
                                         count:(int)count;
@end

namespace {

NSString* const kFolderName = @"folder name";
NSString* const kEmail = @"foo1@gmail.com";

// List of cases to tests.
enum class SignInStatus {
  // The user is signed out.
  kSignOut,
  // The user is signed in and using the local or syncable storage.
  kSignedInOnlyWithLocalOrSyncableStorage,
  // The user is signed in and using the account storage.
  kSignedInOnlyWithAccountStorage,
  // The user is signed in and syncing.
  KSignedInAndSync
};

class BookmarkMediatorUnitTest
    : public BookmarkIOSUnitTestSupport,
      public testing::WithParamInterface<std::tuple<int, bool, SignInStatus>> {
 public:
  void SetUp() override {
    BookmarkIOSUnitTestSupport::SetUp();
    authentication_service_ = AuthenticationServiceFactory::GetForBrowserState(
        chrome_browser_state_.get());

    mediator_ = [[BookmarkMediator alloc]
        initWithWithLocalOrSyncableBookmarkModel:
            local_or_syncable_bookmark_model_
                            accountBookmarkModel:nullptr
                                           prefs:chrome_browser_state_
                                                     ->GetPrefs()
                           authenticationService:authentication_service_
                                     syncService:&sync_service_];
  }

  // Number of bookmark saved.
  int GetBookmarkCountParam() { return std::get<0>(GetParam()); }
  // Weather the bookmarks are saved in the default folder or not.
  bool GetDefaultFolderSetParam() { return std::get<1>(GetParam()); }
  SignInStatus GetSignInStatusParam() { return std::get<2>(GetParam()); }

 protected:
  // Signs in using `fakeIdentity1`.
  FakeSystemIdentity* SignInOnly() {
    FakeSystemIdentityManager* system_identity_manager =
        FakeSystemIdentityManager::FromSystemIdentityManager(
            GetApplicationContext()->GetSystemIdentityManager());
    FakeSystemIdentity* fake_identity = [FakeSystemIdentity fakeIdentity1];
    system_identity_manager->AddIdentity(fake_identity);
    authentication_service_->SignIn(
        fake_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
    return fake_identity;
  }

  // Signs in and enable sync, using the same identity than `SignInOnly()`.
  void SignInAndSync() {
    FakeSystemIdentity* fake_identity = SignInOnly();
    authentication_service_->GrantSyncConsent(
        fake_identity,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_MANAGER);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED` string with `count` value.
  NSString* GetSavedToDeviceText(int count) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count);
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER` string with `count` and
  // `folder_name` value.
  NSString* GetSavedToFolderText(int count, NSString* folder_name) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title",
        base::SysNSStringToUTF16(folder_name));
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT` string with `count` and
  // `email` value.
  NSString* GetSavedToAccountText(int count, NSString* email) {
    std::u16string pattern =
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  // Returns `IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT` string with `count`,
  // `folder_name` and `email` value.
  NSString* GetSavedToFolderToAccountText(int count,
                                          NSString* folder_name,
                                          NSString* email) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT_FOLDER);
    std::u16string message = base::i18n::MessageFormatter::FormatWithNamedArgs(
        pattern, "count", count, "title", base::SysNSStringToUTF16(folder_name),
        "email", base::SysNSStringToUTF16(email));
    return base::SysUTF16ToNSString(message);
  }

  BookmarkMediator* mediator_;
  ChromeAccountManagerService* account_manager_service_;
  AuthenticationService* authentication_service_;
  syncer::TestSyncService sync_service_;
  base::test::ScopedFeatureList scope_;
  base::HistogramTester histogram_tester_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    BookmarkMediatorUnitTest,
    testing::Combine(
        // Number of bookmarked saved.
        testing::Values(1, 2),
        // Bookmark saved in the default folder or not.
        testing::Bool(),
        // Sign-in status.
        testing::Values(SignInStatus::kSignOut,
                        SignInStatus::kSignedInOnlyWithLocalOrSyncableStorage,
                        SignInStatus::kSignedInOnlyWithAccountStorage,
                        SignInStatus::KSignedInAndSync)));

// Tests the snackbar message with all the different combinaisons with:
// * One or two saved bookmarks
// * Using the default folder or not
// * Being signed-out/signed in/signed in with account storage/signed in + sync.
TEST_P(BookmarkMediatorUnitTest, TestSnackBarMessage) {
  const int bookmark_count = GetBookmarkCountParam();
  const SignInStatus signed_in_status = GetSignInStatusParam();
  const bool default_folder_set = GetDefaultFolderSetParam();
  bookmarks::StorageType bookmark_storage_type =
      bookmarks::StorageType::kLocalOrSyncable;
  switch (signed_in_status) {
    case SignInStatus::kSignOut:
      break;
    case SignInStatus::kSignedInOnlyWithLocalOrSyncableStorage:
      SignInOnly();
      break;
    case SignInStatus::kSignedInOnlyWithAccountStorage:
      SignInOnly();
      bookmark_storage_type = bookmarks::StorageType::kAccount;
      break;
    case SignInStatus::KSignedInAndSync:
      SignInAndSync();
      break;
  }
  NSString* const snackbar_message =
      [mediator_ messageForAddingBookmarksInFolder:default_folder_set
                                 folderStorageType:bookmark_storage_type
                                             title:kFolderName
                                             count:bookmark_count];
  NSString* expected_snackbar_message = nil;
  if (signed_in_status == SignInStatus::KSignedInAndSync ||
      bookmark_storage_type == bookmarks::StorageType::kAccount) {
    if (default_folder_set) {
      expected_snackbar_message =
          GetSavedToFolderToAccountText(bookmark_count, kFolderName, kEmail);
    } else {
      expected_snackbar_message = GetSavedToAccountText(bookmark_count, kEmail);
    }
  } else if (default_folder_set) {
    expected_snackbar_message =
        GetSavedToFolderText(bookmark_count, kFolderName);
  } else {
    expected_snackbar_message = GetSavedToDeviceText(bookmark_count);
  }
  ASSERT_NSEQ(snackbar_message, expected_snackbar_message);
}

// Tests bulkAddBookmarksWithURLs with no valid URL passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageNoValidURLs) {
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@""] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(0U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"0 Bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with one valid URL passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageOneValidURL) {
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@"https://google.ca"] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(1U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"Bookmark saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 1, 1);
}

// Tests bulkAddBookmarksWithURLs with two valid URLs passed.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageTwoValidURLs) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"https://google.fr"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(2U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"2 Bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 2, 1);
}

// Tests bulkAddBookmarksWithURLs with a set of mixed valid and invalid URLs.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageValidAndInvalidURLs) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"3 Bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
}

// Tests bulkAddBookmarksWithURLs with duplicate bookmarks.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageDuplicateBookmarks) {
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text, @"3 Bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);

  // Try bulk adding the same URLs again, none should be added.
  MDCSnackbarMessage* const snackbarMessageDuplicates =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks_dupes =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks_dupes.size());
  ASSERT_NSEQ(snackbarMessageDuplicates.text, @"0 Bookmarks saved");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with no valid URL passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageNoValidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@""] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(0U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"0 Bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

// Tests bulkAddBookmarksWithURLs with one valid URL passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageOneValidURLSyncing) {
  SignInAndSync();
  NSArray* URLs = @[ [[NSURL alloc] initWithString:@"https://google.ca"] ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(1U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"Bookmark saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 1, 1);
}

// Tests bulkAddBookmarksWithURLs with two valid URLs passed while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest, TestBulkSnackbarMessageTwoValidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"https://google.fr"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(2U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"2 Bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 2, 1);
}

// Tests bulkAddBookmarksWithURLs with a set of mixed valid and invalid URLs
// while signed in and syncing.
TEST_F(BookmarkMediatorUnitTest,
       TestBulkSnackbarMessageValidAndInvalidURLsSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"3 Bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
}

// Tests bulkAddBookmarksWithURLs with duplicate bookmarks while signed in and
// syncing.
TEST_F(BookmarkMediatorUnitTest,
       TestBulkSnackbarMessageDuplicateBookmarksSyncing) {
  SignInAndSync();
  NSArray* URLs = @[
    [[NSURL alloc] initWithString:@"https://google.com"],
    [[NSURL alloc] initWithString:@"::invalid::"],
    [[NSURL alloc] initWithString:@"https://google.fr"],
    [[NSURL alloc] initWithString:@"https://google.co.jp"]
  ];

  MDCSnackbarMessage* const snackbarMessage =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks.size());
  ASSERT_NSEQ(snackbarMessage.text,
              @"3 Bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);

  // Try bulk adding the same URLs again, none should be added.
  MDCSnackbarMessage* const snackbarMessageDuplicates =
      [mediator_ bulkAddBookmarksWithURLs:URLs
                               viewAction:^{
                               }];

  std::vector<bookmarks::UrlAndTitle> bookmarks_dupes =
      local_or_syncable_bookmark_model_->GetUniqueUrls();

  ASSERT_EQ(3U, bookmarks_dupes.size());
  ASSERT_NSEQ(snackbarMessageDuplicates.text,
              @"0 Bookmarks saved in your Google Account, foo1@gmail.com");
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 3, 1);
  histogram_tester_.ExpectBucketCount("IOS.Bookmarks.BulkAddURLsCount", 0, 1);
}

}  // namespace
