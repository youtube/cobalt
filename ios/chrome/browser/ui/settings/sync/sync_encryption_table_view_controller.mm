// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"

#import <memory>

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/base/command_line_switches.h"
#import "components/sync/base/sync_prefs.h"
#import "components/sync/driver/sync_service.h"
#import "components/sync/driver/sync_user_settings.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/sync_observer_bridge.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/settings_controller_protocol.h"
#import "ios/chrome/browser/ui/settings/sync/sync_create_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierEncryption = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAccount = kItemTypeEnumZero,
  ItemTypePassphrase,
  ItemTypeFooter,
};

}  // namespace

@interface SyncEncryptionTableViewController () <SyncObserverModelBridge,
                                                 SettingsControllerProtocol> {
  std::unique_ptr<SyncObserverBridge> _syncObserver;
  BOOL _isUsingExplicitPassphrase;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@property(nonatomic, assign, readonly) Browser* browser;

@end

@implementation SyncEncryptionTableViewController

@synthesize browser = _browser;

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _browser = browser;
    ChromeBrowserState* browserState = self.browser->GetBrowserState();
    self.title = l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_TITLE);
    syncer::SyncService* syncService =
        SyncServiceFactory::GetForBrowserState(browserState);
    _isUsingExplicitPassphrase =
        syncService->IsEngineInitialized() &&
        syncService->GetUserSettings()->IsUsingExplicitPassphrase();
    _syncObserver = std::make_unique<SyncObserverBridge>(self, syncService);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedSectionFooterHeight =
      kTableViewHeaderFooterViewHeight;
  [self loadModel];
}

#pragma mark - SettingsRootTableViewController

- (void)loadModel {
  [super loadModel];
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierEncryption];
  [model addItem:[self accountItem]
      toSectionWithIdentifier:SectionIdentifierEncryption];
  [model addItem:[self passphraseItem]
      toSectionWithIdentifier:SectionIdentifierEncryption];

  if (_isUsingExplicitPassphrase) {
    [model setFooter:[self footerItem]
        forSectionWithIdentifier:SectionIdentifierEncryption];
  }
}

#pragma mark - Items

// Returns an account item.
- (TableViewItem*)accountItem {
  DCHECK(syncer::IsSyncAllowedByFlag());
  NSString* text = l10n_util::GetNSString(IDS_SYNC_BASIC_ENCRYPTION_DATA);
  return [self itemWithType:ItemTypeAccount
                       text:text
                    checked:!_isUsingExplicitPassphrase
                    enabled:!_isUsingExplicitPassphrase];
}

// Returns a passphrase item.
- (TableViewItem*)passphraseItem {
  DCHECK(syncer::IsSyncAllowedByFlag());
  NSString* text = l10n_util::GetNSString(IDS_SYNC_FULL_ENCRYPTION_DATA);
  return [self itemWithType:ItemTypePassphrase
                       text:text
                    checked:_isUsingExplicitPassphrase
                    enabled:!_isUsingExplicitPassphrase];
}

// Returns a footer item with a link.
- (TableViewHeaderFooterItem*)footerItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_SYNC_ENCRYPTION_PASSPHRASE_HINT);
  footerItem.urls = @[ [[CrURL alloc]
      initWithGURL:google_util::AppendGoogleLocaleParam(
                       GURL(kSyncGoogleDashboardURL),
                       GetApplicationContext()->GetApplicationLocale())] ];
  return footerItem;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* footerView = [super tableView:tableView
                 viewForFooterInSection:section];
  if (SectionIdentifierEncryption ==
          [self.tableViewModel sectionIdentifierForSectionIndex:section] &&
      [self.tableViewModel footerForSectionIndex:section]) {
    TableViewLinkHeaderFooterView* footer =
        base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(footerView);
    footer.delegate = self;
  }
  return footerView;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK(!_settingsAreDismissed) << "tableView:didSelectRowAtIndexPath called "
                                    "after -settingsWillBeDismissed";
  DCHECK_EQ(indexPath.section,
            [self.tableViewModel
                sectionForSectionIdentifier:SectionIdentifierEncryption]);

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypePassphrase: {
      DCHECK(syncer::IsSyncAllowedByFlag());
      ChromeBrowserState* browserState = self.browser->GetBrowserState();
      syncer::SyncService* service =
          SyncServiceFactory::GetForBrowserState(browserState);
      if (service->IsEngineInitialized() &&
          !service->GetUserSettings()->IsUsingExplicitPassphrase()) {
        SyncCreatePassphraseTableViewController* controller =
            [[SyncCreatePassphraseTableViewController alloc]
                initWithBrowser:self.browser];
        if (controller) {
          controller.dispatcher = self.dispatcher;
          [self.navigationController pushViewController:controller
                                               animated:YES];
        }
      }
      break;
    }
    case ItemTypeAccount:
    case ItemTypeFooter:
    default:
      break;
  }

  [tableView deselectRowAtIndexPath:indexPath animated:NO];
}

#pragma mark - SettingsControllerProtocol callbacks

- (void)reportDismissalUserAction {
  base::RecordAction(
      base::UserMetricsAction("MobileSyncEncryptionSettingsClose"));
}

- (void)reportBackUserAction {
  NOTREACHED();
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  // Remove observer bridges.
  _syncObserver.reset();

  // Clear C++ ivars.
  _browser = nil;

  _settingsAreDismissed = YES;
}

#pragma mark SyncObserverModelBridge

- (void)onSyncStateChanged {
  DCHECK(!_settingsAreDismissed)
      << "onSyncStateChanged called after -settingsWillBeDismissed";
  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  syncer::SyncService* service =
      SyncServiceFactory::GetForBrowserState(browserState);
  BOOL isNowUsingExplicitPassphrase =
      service->IsEngineInitialized() &&
      service->GetUserSettings()->IsUsingExplicitPassphrase();
  if (_isUsingExplicitPassphrase != isNowUsingExplicitPassphrase) {
    _isUsingExplicitPassphrase = isNowUsingExplicitPassphrase;
    [self reloadData];
  }
}

#pragma mark - Private methods

- (TableViewItem*)itemWithType:(NSInteger)type
                          text:(NSString*)text
                       checked:(BOOL)checked
                       enabled:(BOOL)enabled {
  TableViewTextItem* item = [[TableViewTextItem alloc] initWithType:type];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.text = text;
  item.accessoryType = checked ? UITableViewCellAccessoryCheckmark
                               : UITableViewCellAccessoryNone;
  item.textColor = enabled ? [UIColor colorNamed:kTextPrimaryColor]
                           : [UIColor colorNamed:kTextSecondaryColor];
  item.enabled = enabled;
  return item;
}

@end
