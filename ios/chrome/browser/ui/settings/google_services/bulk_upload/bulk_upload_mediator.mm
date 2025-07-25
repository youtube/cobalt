// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mediator.h"

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/user_metrics.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/signin/public/base/consent_level.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/signin/public/identity_manager/primary_account_change_event.h"
#import "components/sync/base/model_type.h"
#import "components/sync/service/local_data_description.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/signin/system_identity.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_consumer.h"
#import "ios/chrome/browser/ui/settings/google_services/bulk_upload/bulk_upload_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

struct BulkUploadModelItem {
  syncer::ModelType model_type;
  BulkUploadType bulk_upload_type;
  int title_string_id;
  int subtitle_string_id;
  NSString* const view_accessibility_id;
};

// List of model type to display for the bulk upload. The order will be used
// in the table view.
const BulkUploadModelItem kBuildUploadModelItems[] = {
    {
        syncer::BOOKMARKS,
        BulkUploadType::kBookmark,
        IDS_IOS_BULK_UPLOAD_BOOKMARK_TITLE,
        IDS_IOS_BULK_UPLOAD_BOOKMARK_SUBTITLE,
        kBulkUploadTableViewBookmarksItemAccessibilityIdentifer,
    },
    {
        syncer::PASSWORDS,
        BulkUploadType::kPassword,
        IDS_IOS_BULK_UPLOAD_PASSWORD_TITLE,
        IDS_IOS_BULK_UPLOAD_BOOKMARK_SUBTITLE,
        kBulkUploadTableViewPasswordsItemAccessibilityIdentifer,
    },
    {
        syncer::READING_LIST,
        BulkUploadType::kReadinglist,
        IDS_IOS_BULK_UPLOAD_READING_LIST_TITLE,
        IDS_IOS_BULK_UPLOAD_READING_LIST_SUBTITLE,
        kBulkUploadTableViewReadingListItemAccessibilityIdentifer,
    },
};

}  // namespace

@interface BulkUploadMediator () <IdentityManagerObserverBridgeDelegate>
@end

@implementation BulkUploadMediator {
  // The identity manager for this service
  signin::IdentityManager* _identityManager;
  // The email of the user, to display in snackbar message.
  // Observes changes in identity.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;
  // The sync service.
  syncer::SyncService* _syncService;
  // Set of BulkModelType whose item is selected.
  std::set<BulkUploadType> _selectedTypes;
  // Map returned by syncServer::GetLocalDataDescriptions, associating to each
  // type the descripton of the elements to upload.
  std::map<syncer::ModelType, syncer::LocalDataDescription> _map;
  // Provides the face id or password authentication. It is required to bulk
  // upload passwords.
  // _reauthenticationModule needs to be retained until the callback is called.
  ReauthenticationModule* _reauthenticationModule;
}

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
                    identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  CHECK(syncService);
  CHECK(identityManager);
  if (self) {
    _syncService = syncService;
    _identityManager = identityManager;
    _identityObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(identityManager, self));
  }

  return self;
}

- (void)dealloc {
  DCHECK(!_syncService);
}

- (void)disconnect {
  _identityObserverBridge.reset();
  _syncService = nullptr;
}

- (void)setConsumer:(id<BulkUploadConsumer>)consumer {
  _consumer = consumer;
  if (!consumer) {
    return;
  }
  __weak BulkUploadMediator* weakSelf = self;
  syncer::ModelTypeSet modelTypeSet;
  for (BulkUploadModelItem modelItem : kBuildUploadModelItems) {
    modelTypeSet.Put(modelItem.model_type);
  }
  _syncService->GetLocalDataDescriptions(
      modelTypeSet,
      base::BindOnce(
          ^(std::map<syncer::ModelType, syncer::LocalDataDescription> map) {
            [weakSelf updateConsumer:map];
          }));
}

#pragma mark - Private

- (void)save {
  syncer::ModelTypeSet selectedModelTypes = [self selectedModelTypeEnumSet];
  _syncService->TriggerLocalDataMigration(selectedModelTypes);
  int count = 0;
  // Count items for the selected types.
  for (syncer::ModelType model_type : selectedModelTypes) {
    count += _map[model_type].item_count;
  }
  const std::string email =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .email;
  [self.delegate
      displayInSnackbar:base::SysUTF16ToNSString(
                            base::i18n::MessageFormatter::FormatWithNamedArgs(
                                l10n_util::GetStringUTF16(
                                    IDS_IOS_BULK_UPLOAD_SNACKBAR_MESSAGE),
                                "count", count, "email", email))

  ];
  [self.delegate mediatorWantsToBeDismissed:self];
}

// Processes the data and request the consumer to update its view accordingly.
// The map is assumed to contain data for all types the batch upload process can
// work with.
- (void)updateConsumer:
    (std::map<syncer::ModelType, syncer::LocalDataDescription>)map {
  _map = map;
  NSMutableArray<BulkUploadViewItem*>* viewItems = [NSMutableArray array];
  for (BulkUploadModelItem modelItem : kBuildUploadModelItems) {
    if (_map[modelItem.model_type].item_count == 0) {
      continue;
    }
    _selectedTypes.insert(modelItem.bulk_upload_type);
    BulkUploadViewItem* viewItem =
        [self generateBulkUploadItemWithModelItem:modelItem];
    [viewItems addObject:viewItem];
  }
  [self.consumer updateViewWithViewItems:[viewItems copy]];
  [self updateButtonEnabledState];
}

// Process a single data type.
// `enabled` is the property that tracks whether this data is enabled.
// `oneElementId`, `twoElements`, `threeElementsId` are the id of the strings to
// display when there is one, two or three elements to display.
// `consumerUpdater` is the block that update the number and text of the view
- (BulkUploadViewItem*)generateBulkUploadItemWithModelItem:
    (BulkUploadModelItem)modelItem {
  syncer::LocalDataDescription description = _map[modelItem.model_type];
  CHECK_GT(description.domains.size(), 0ul)
      << "model type: " << static_cast<int>(modelItem.model_type);
  NSString* subtitle;
  if (description.domains.size() == 1) {
    subtitle = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(modelItem.subtitle_string_id), "count",
            static_cast<int>(description.domain_count), "website_1",
            description.domains[0], "more_count",
            static_cast<int>(description.domain_count - 1)));
  } else {
    subtitle = base::SysUTF16ToNSString(
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            l10n_util::GetStringUTF16(modelItem.subtitle_string_id), "count",
            static_cast<int>(description.domain_count), "website_1",
            description.domains[0], "website_2", description.domains[1],
            "more_count", static_cast<int>(description.domain_count - 2)));
  }
  BulkUploadViewItem* bulkUploadViewItem = [[BulkUploadViewItem alloc] init];
  bulkUploadViewItem.type = modelItem.bulk_upload_type;
  bulkUploadViewItem.title =
      base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
          modelItem.title_string_id, description.item_count));
  bulkUploadViewItem.subtitle = subtitle;
  bulkUploadViewItem.selected =
      base::Contains(_selectedTypes, modelItem.bulk_upload_type);
  bulkUploadViewItem.accessibilityIdentifier = modelItem.view_accessibility_id;
  return bulkUploadViewItem;
}

// Updates the enabled state of the Save in Account button
- (void)updateButtonEnabledState {
  syncer::ModelTypeSet selectedModelTypes = [self selectedModelTypeEnumSet];
  self.consumer.validationButtonEnabled = selectedModelTypes.Size() > 0;
}

#pragma mark - BulkUploadMutator

- (void)bulkUploadViewItemWithType:(BulkUploadType)type
                        isSelected:(BOOL)selected {
  if (selected) {
    _selectedTypes.insert(type);
  } else {
    _selectedTypes.erase(type);
  }
  [self updateButtonEnabledState];
}

- (void)requestSave {
  base::RecordAction(base::UserMetricsAction("Signin_BulkUpload_Save"));
  // The user must authenticate if and only if they request to upload passwords.
  syncer::ModelTypeSet selectedModelTypes = [self selectedModelTypeEnumSet];
  if (!selectedModelTypes.Has(syncer::ModelType::PASSWORDS)) {
    [self save];
    return;
  }
  // Use util from password_utils to create reauth module to allow easy testing.
  _reauthenticationModule = password_manager::BuildReauthenticationModule();
  if (![_reauthenticationModule canAttemptReauth]) {
    base::RecordAction(
        base::UserMetricsAction("Signin_BulkUpload_FaceID_CannotBeStarted"));
    return;
  }
  __weak BulkUploadMediator* weakSelf = self;
  // The message to request authentification must mention the app will upload
  // passwords. If there are other types, they also get mentionned, even if they
  // don’t formally require authentication. This is because, if the
  // authentication fails, those other types are not saved either.
  int authentification_identifier =
      (selectedModelTypes.Size() == 1)
          ? IDS_IOS_BULK_UPLOAD_PASSWORD_AUTHENTIFY_MESSAGE
          : IDS_IOS_BULK_UPLOAD_PASSWORD_AND_OTHER_TYPE_AUTHENTIFY_MESSAGE;
  [_reauthenticationModule
      attemptReauthWithLocalizedReason:l10n_util::GetNSString(
                                           authentification_identifier)
                  canReusePreviousAuth:NO
                               handler:^(ReauthenticationResult result) {
                                 [weakSelf
                                     reauthenticationModuleDidFinishWithResult:
                                         result];
                               }];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  if (event.GetEventTypeFor(signin::ConsentLevel::kSignin) !=
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    [self.delegate mediatorWantsToBeDismissed:self];
  }
}

#pragma mark - Private

// Returns model type set of the selected data types.
- (syncer::ModelTypeSet)selectedModelTypeEnumSet {
  syncer::ModelTypeSet modelTypeSet;
  for (BulkUploadModelItem modelItem : kBuildUploadModelItems) {
    if (base::Contains(_selectedTypes, modelItem.bulk_upload_type) &&
        _map[modelItem.model_type].item_count > 0) {
      modelTypeSet.Put(modelItem.model_type);
    }
  }
  return modelTypeSet;
}

// Called when `_reauthenticationModule` is finished.
- (void)reauthenticationModuleDidFinishWithResult:
    (ReauthenticationResult)result {
  CHECK(_reauthenticationModule);
  _reauthenticationModule = nil;
  switch (result) {
    case ReauthenticationResult::kSuccess: {
      base::RecordAction(
          base::UserMetricsAction("Signin_BulkUpload_FaceID_Success"));
      [self save];
      break;
    }
    case ReauthenticationResult::kFailure: {
      base::RecordAction(
          base::UserMetricsAction("Signin_BulkUpload_FaceID_Failed"));
      // TODO(crbug.com/1477699): Warns the user.
      break;
    }
    case ReauthenticationResult::kSkipped: {
      // This should not happens since `canReusePreviousAuth` is set to `NO`.
      NOTREACHED_NORETURN();
    }
  }
}

@end
