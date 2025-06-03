// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/i18n/message_formatter.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/bookmarks/browser/bookmark_node.h"
#import "components/bookmarks/browser/bookmark_utils.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/pref_service.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/bookmarks/model/bookmarks_utils.h"
#import "ios/chrome/browser/bookmarks/model/local_or_syncable_bookmark_model_factory.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/ntp/metrics/home_metrics.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

@implementation BookmarkMediator {
  // Profile bookmark model for this mediator.
  base::WeakPtr<bookmarks::BookmarkModel> _localOrSyncableBookmarkModel;
  // Account bookmark model for this mediator.
  base::WeakPtr<bookmarks::BookmarkModel> _accountBookmarkModel;

  // Prefs model for this mediator.
  PrefService* _prefs;

  // Authentication service for this mediator.
  base::WeakPtr<AuthenticationService> _authenticationService;

  // Sync service for this mediator.
  syncer::SyncService* _syncService;
}

+ (void)registerBrowserStatePrefs:(user_prefs::PrefRegistrySyncable*)registry {
  registry->RegisterInt64Pref(
      prefs::kIosBookmarkLastUsedFolderReceivingBookmarks,
      kLastUsedBookmarkFolderNone);
  registry->RegisterIntegerPref(
      prefs::kIosBookmarkLastUsedStorageReceivingBookmarks,
      static_cast<int>(bookmarks::StorageType::kLocalOrSyncable));
}

- (instancetype)
    initWithWithLocalOrSyncableBookmarkModel:
        (bookmarks::BookmarkModel*)localOrSyncableBookmarkModel
                        accountBookmarkModel:
                            (bookmarks::BookmarkModel*)accountBookmarkModel
                                       prefs:(PrefService*)prefs
                       authenticationService:
                           (AuthenticationService*)authenticationService
                                 syncService:(syncer::SyncService*)syncService {
  self = [super init];
  if (self) {
    _localOrSyncableBookmarkModel = localOrSyncableBookmarkModel->AsWeakPtr();
    if (accountBookmarkModel) {
      _accountBookmarkModel = accountBookmarkModel->AsWeakPtr();
    }
    _prefs = prefs;
    _authenticationService = authenticationService->GetWeakPtr();
    _syncService = syncService;
  }
  return self;
}

- (void)disconnect {
  _localOrSyncableBookmarkModel = nullptr;
  _accountBookmarkModel = nullptr;
  _prefs = nullptr;
  _authenticationService = nullptr;
  _syncService = nullptr;
}

- (MDCSnackbarMessage*)addBookmarkWithTitle:(NSString*)title
                                        URL:(const GURL&)URL
                                 editAction:(void (^)())editAction {
  RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts);
  base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
  LogBookmarkUseForDefaultBrowserPromo();

  const BookmarkNode* defaultFolder = GetDefaultBookmarkFolder(
      _prefs, bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService),
      _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get());
  BookmarkModel* modelForDefaultFolder =
      bookmark_utils_ios::GetBookmarkModelForNode(
          defaultFolder, _localOrSyncableBookmarkModel.get(),
          _accountBookmarkModel.get());
  modelForDefaultFolder->AddNewURL(defaultFolder,
                                   defaultFolder->children().size(),
                                   base::SysNSStringToUTF16(title), URL);

  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = editAction;
  action.title =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_SNACKBAR_EDIT_BOOKMARK);
  action.accessibilityIdentifier = @"Edit";

  NSString* folderTitle =
      bookmark_utils_ios::TitleForBookmarkNode(defaultFolder);
  bookmarks::StorageType storageType = bookmark_utils_ios::GetBookmarkModelType(
      defaultFolder, _localOrSyncableBookmarkModel.get(),
      _accountBookmarkModel.get());
  NSString* text = [self
      messageForAddingBookmarksInFolder:!IsLastUsedBookmarkFolderSet(_prefs)
                      folderStorageType:storageType
                                  title:folderTitle
                                  count:1];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.action = action;
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

- (MDCSnackbarMessage*)bulkAddBookmarksWithURLs:(NSArray<NSURL*>*)URLs
                                     viewAction:(void (^)())viewAction {
  DCHECK([URLs count] > 0);
  base::RecordAction(base::UserMetricsAction("IOSBookmarksAddedInBulk"));

  const BookmarkNode* defaultFolder = GetDefaultBookmarkFolder(
      _prefs, bookmark_utils_ios::IsAccountBookmarkStorageOptedIn(_syncService),
      _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get());
  BookmarkModel* modelForDefaultFolder =
      bookmark_utils_ios::GetBookmarkModelForNode(
          defaultFolder, _localOrSyncableBookmarkModel.get(),
          _accountBookmarkModel.get());

  // Add bookmarks and keep track of successful additions.
  int successfullyAddedBookmarks = 0;
  for (NSURL* NSURL in URLs) {
    GURL URL = net::GURLWithNSURL(NSURL);

    if (!URL.is_valid()) {
      continue;
    }

    // Construct the title from domain + path (stripping trailing slash from
    // path).
    std::string path = URL.GetWithoutRef().GetWithoutFilename().path();
    if (path.length() > 0) {
      path.pop_back();
    }
    NSString* title = base::SysUTF8ToNSString(URL.host() + path);

    const BookmarkNode* existingBookmark =
        bookmark_utils_ios::GetMostRecentlyAddedUserNodeForURL(
            URL, _localOrSyncableBookmarkModel.get(),
            _accountBookmarkModel.get());

    if (!existingBookmark) {
      modelForDefaultFolder->AddNewURL(defaultFolder,
                                       defaultFolder->children().size(),
                                       base::SysNSStringToUTF16(title), URL);
      successfullyAddedBookmarks++;
    }
  }

  base::UmaHistogramCounts100("IOS.Bookmarks.BulkAddURLsCount",
                              successfullyAddedBookmarks);

  // Create snackbar message.
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = viewAction;
  action.title =
      l10n_util::GetNSString(IDS_IOS_BOOKMARK_SNACKBAR_VIEW_BOOKMARKS);

  bookmarks::StorageType storageType = bookmark_utils_ios::GetBookmarkModelType(
      defaultFolder, _localOrSyncableBookmarkModel.get(),
      _accountBookmarkModel.get());
  NSString* result = [self
      messageForBulkAddingBookmarksWithStorageType:storageType
                        successfullyAddedBookmarks:successfullyAddedBookmarks];

  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:result];
  message.action = action;
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;

  return message;
}

- (MDCSnackbarMessage*)addBookmarks:(NSArray<URLWithTitle*>*)URLs
                           toFolder:(const BookmarkNode*)folder {
  LogBookmarkUseForDefaultBrowserPromo();

  BookmarkModel* modelForFolder = bookmark_utils_ios::GetBookmarkModelForNode(
      folder, _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get());
  for (URLWithTitle* urlWithTitle in URLs) {
    RecordModuleFreshnessSignal(ContentSuggestionsModuleType::kShortcuts);
    base::RecordAction(base::UserMetricsAction("BookmarkAdded"));
    modelForFolder->AddNewURL(folder, folder->children().size(),
                              base::SysNSStringToUTF16(urlWithTitle.title),
                              urlWithTitle.URL);
  }

  NSString* folderTitle = bookmark_utils_ios::TitleForBookmarkNode(folder);
  bookmarks::StorageType storageType = bookmark_utils_ios::GetBookmarkModelType(
      folder, _localOrSyncableBookmarkModel.get(), _accountBookmarkModel.get());
  NSString* text = [self messageForAddingBookmarksInFolder:(folderTitle.length)
                                         folderStorageType:storageType
                                                     title:folderTitle
                                                     count:URLs.count];
  TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.category = bookmark_utils_ios::kBookmarksSnackbarCategory;
  return message;
}

#pragma mark - Private

// The bookmark is saved in the account if either following condition is true:
// * the saved folder is in the account model,
// * the sync consent has been granted and the bookmark data type is enabled
- (BOOL)bookmarkSavedIntoAccountWithStorageType:
    (bookmarks::StorageType)storageType {
  // TODO(crbug.com/1462552): Simplify once kSync becomes unreachable or is
  // deleted from the codebase. See ConsentLevel::kSync documentation for
  // details.
  BOOL hasSyncConsent =
      _authenticationService->HasPrimaryIdentity(signin::ConsentLevel::kSync);
  BOOL savedIntoAccount =
      (storageType == bookmarks::StorageType::kAccount) ||
      (hasSyncConsent &&
       _syncService->GetUserSettings()->GetSelectedTypes().Has(
           syncer::UserSelectableType::kBookmarks));
  return savedIntoAccount;
}

// The localized strings for adding bookmarks.
// `addFolder`: whether the folder name should appear in the message
// `folderTitle`: The name of the folder. Assumed to be non-nil if `addFolder`
// is true. `count`: the number of bookmarks. Used for localization.
- (NSString*)messageForAddingBookmarksInFolder:(BOOL)addFolder
                             folderStorageType:
                                 (bookmarks::StorageType)storageType
                                         title:(NSString*)folderTitle
                                         count:(int)count {
  std::u16string result;
  id<SystemIdentity> identity =
      _authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  BOOL savedIntoAccount =
      [self bookmarkSavedIntoAccountWithStorageType:storageType];
  if (savedIntoAccount) {
    std::u16string email = base::SysNSStringToUTF16(identity.userEmail);
    if (addFolder) {
      std::u16string title = base::SysNSStringToUTF16(folderTitle);
      std::u16string pattern = l10n_util::GetStringUTF16(
          IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT_FOLDER);
      result = base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "count", count, "title", title, "email", email);
    } else {
      std::u16string pattern =
          l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_INTO_ACCOUNT);
      result = base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "count", count, "email", email);
    }
  } else {
    if (addFolder) {
      std::u16string title = base::SysNSStringToUTF16(folderTitle);
      std::u16string pattern =
          l10n_util::GetStringUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED_FOLDER);
      result = base::i18n::MessageFormatter::FormatWithNamedArgs(
          pattern, "count", count, "title", title);
    } else {
      result =
          l10n_util::GetPluralStringFUTF16(IDS_IOS_BOOKMARK_PAGE_SAVED, count);
    }
  }
  return base::SysUTF16ToNSString(result);
}

// The localized string that appears to users for bulk adding bookmarks.
- (NSString*)messageForBulkAddingBookmarksWithStorageType:
                 (bookmarks::StorageType)storageType
                               successfullyAddedBookmarks:(int)count {
  std::u16string result;

  BOOL savedIntoAccount =
      [self bookmarkSavedIntoAccountWithStorageType:storageType];
  if (savedIntoAccount) {
    id<SystemIdentity> identity = _authenticationService->GetPrimaryIdentity(
        signin::ConsentLevel::kSignin);
    result = base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARKS_BULK_SAVED_ACCOUNT),
        "count", count, "email", base::SysNSStringToUTF16(identity.userEmail));
  } else {
    result = base::i18n::MessageFormatter::FormatWithNamedArgs(
        l10n_util::GetStringUTF16(IDS_IOS_BOOKMARKS_BULK_SAVED), "count",
        count);
  }

  return base::SysUTF16ToNSString(result);
}
@end
