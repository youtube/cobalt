// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/prefs/pref_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browsing_data_commands.h"
#import "ios/chrome/browser/shared/ui/elements/chrome_activity_overlay_coordinator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/signout_action_sheet_coordinator.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ClearBrowsingDataTableViewController () <
    ClearBrowsingDataConsumer,
    IdentityManagerObserverBridgeDelegate,
    SignoutActionSheetCoordinatorDelegate,
    TableViewLinkHeaderFooterItemDelegate,
    UIGestureRecognizerDelegate>

// TODO(crbug.com/850699): remove direct dependency and replace with
// delegate.
@property(nonatomic, readonly, strong) ClearBrowsingDataManager* dataManager;

// Browser state.
@property(nonatomic, readonly) ChromeBrowserState* browserState;

// Browser.
@property(nonatomic, readonly) Browser* browser;

// Coordinator that managers a UIAlertController to clear browsing data.
@property(nonatomic, strong) ActionSheetCoordinator* actionSheetCoordinator;

// Coordinator for displaying a modal overlay with native activity indicator to
// prevent the user from interacting with the page.
@property(nonatomic, strong)
    ChromeActivityOverlayCoordinator* overlayCoordinator;

@property(nonatomic, readonly, strong)
    UIBarButtonItem* clearBrowsingDataBarButton;

// Modal alert for Browsing history removed dialog.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The data manager might want to reload tableView cells before the tableView
// has loaded, we need to prevent this kind of updates until the tableView
// loads.
@property(nonatomic, assign) BOOL suppressTableViewUpdates;

@end

@implementation ClearBrowsingDataTableViewController {
  // Modal alert for sign out.
  SignoutActionSheetCoordinator* _signoutCoordinator;
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
}
@synthesize dispatcher = _dispatcher;
@synthesize delegate = _delegate;
@synthesize clearBrowsingDataBarButton = _clearBrowsingDataBarButton;
#pragma mark - ViewController Lifecycle.

- (instancetype)initWithBrowser:(Browser*)browser {
  UITableViewStyle style = ChromeTableViewStyle();
  self = [super initWithStyle:style];
  if (self) {
    _browser = browser;
    _browserState = browser->GetBrowserState();
    _dataManager = [[ClearBrowsingDataManager alloc]
        initWithBrowserState:browser->GetBrowserState()];
    _dataManager.consumer = self;
    _identityManagerObserverBridge.reset(
        new signin::IdentityManagerObserverBridge(
            IdentityManagerFactory::GetForBrowserState(_browserState), self));
  }
  return self;
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (!parent) {
    [self.delegate clearBrowsingDataTableViewControllerWasRemoved:self];
  }
}

#pragma mark - Property

- (UIBarButtonItem*)clearBrowsingDataBarButton {
  if (!_clearBrowsingDataBarButton) {
    _clearBrowsingDataBarButton = [[UIBarButtonItem alloc]
        initWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)
                style:UIBarButtonItemStylePlain
               target:self
               action:@selector(showClearBrowsingDataAlertController:)];
    _clearBrowsingDataBarButton.accessibilityIdentifier =
        kClearBrowsingDataButtonIdentifier;
    _clearBrowsingDataBarButton.tintColor = [UIColor colorNamed:kRedColor];
  }
  return _clearBrowsingDataBarButton;
}

#pragma mark - IdentityManagerObserverBridgeDelegate

// Update footer to take into account whether the user is signed-in or not.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  [self.dataManager updateModel:self.tableViewModel
                  withTableView:self.tableView];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIBarButtonItem* flexibleSpace = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                           target:nil
                           action:nil];
  [self setToolbarItems:@[
    flexibleSpace, self.clearBrowsingDataBarButton, flexibleSpace
  ]
               animated:YES];

  self.tableView.accessibilityIdentifier =
      kClearBrowsingDataViewAccessibilityIdentifier;

  // Navigation controller configuration.
  self.title = l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE);
  // Adds the "Done" button and hooks it up to `dismiss`.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismiss)];
  dismissButton.accessibilityIdentifier = kSettingsDoneButtonId;
  self.navigationItem.rightBarButtonItem = dismissButton;

  // Do not allow any TableView updates until the model is fully loaded. The
  // model might try re-loading some cells and the TableView might not be loaded
  // at this point (https://crbug.com/873929).
  self.suppressTableViewUpdates = YES;
  [self loadModel];
  self.suppressTableViewUpdates = NO;
  [self.dataManager prepare];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self.dataManager restartCounters:BrowsingDataRemoveMask::REMOVE_ALL];

  [self updateToolbarButtons];
  // Showing toolbar here because parent class hides toolbar in
  // viewWillDisappear:.
  self.navigationController.toolbarHidden = NO;
}

- (void)loadModel {
  [super loadModel];
  [self.dataManager loadModel:self.tableViewModel];
}

- (void)dismiss {
  base::RecordAction(base::UserMetricsAction("MobileClearBrowsingDataClose"));
  [self prepareForDismissal];
  [self.delegate dismissClearBrowsingData];
}

#pragma mark - Public Methods

- (void)prepareForDismissal {
  if (self.actionSheetCoordinator) {
    [self.actionSheetCoordinator stop];
    self.actionSheetCoordinator = nil;
  }
  if (self.alertCoordinator) {
    [self.alertCoordinator stop];
    self.alertCoordinator = nil;
  }
  if (self.overlayCoordinator.started) {
    [self.overlayCoordinator stop];
    self.navigationController.interactivePopGestureRecognizer.delegate = nil;
    self.overlayCoordinator = nil;
  }
  _identityManagerObserverBridge.reset();
  [self.dataManager disconnect];
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizerShouldBegin:(UIGestureRecognizer*)gestureRecognizer {
  if (gestureRecognizer ==
      self.navigationController.interactivePopGestureRecognizer) {
    // This view controller should only be observing gestures when the activity
    // overlay is showing (e.g. when Clear Browsing Data is in progress and the
    // user should not be able to swipe away from this view).
    return NO;
  }
  return YES;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cellToReturn = [super tableView:tableView
                             cellForRowAtIndexPath:indexPath];
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill:
      // For these cells the selection style application is specified in the
      // corresponding item definition.
      cellToReturn.selectionStyle = UITableViewCellSelectionStyleNone;
      break;
    default:
      break;
  }
  return cellToReturn;
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierSavedSiteData:
    case SectionIdentifierGoogleAccount: {
      TableViewLinkHeaderFooterView* linkView =
          base::mac::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
    default:
      break;
  }
  return view;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case SectionIdentifierGoogleAccount:
    case SectionIdentifierSavedSiteData:
      return 5;
    default:
      return [super tableView:tableView heightForHeaderInSection:section];
  }
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  switch (item.type) {
    case ItemTypeTimeRange: {
      UIViewController* controller =
          [[TimeRangeSelectorTableViewController alloc]
              initWithPrefs:self.browserState->GetPrefs()];
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      [self.navigationController pushViewController:controller animated:YES];
      break;
    }
    case ItemTypeDataTypeBrowsingHistory:
    case ItemTypeDataTypeCookiesSiteData:
    case ItemTypeDataTypeCache:
    case ItemTypeDataTypeSavedPasswords:
    case ItemTypeDataTypeAutofill: {
      DCHECK([item isKindOfClass:[TableViewClearBrowsingDataItem class]]);
      TableViewClearBrowsingDataItem* clearBrowsingDataItem =
          base::mac::ObjCCastStrict<TableViewClearBrowsingDataItem>(item);

      self.browserState->GetPrefs()->SetBoolean(clearBrowsingDataItem.prefName,
                                                !clearBrowsingDataItem.checked);
      // UI update will be trigerred by data manager.
      break;
    }
    default:
      break;
  }
  [self updateToolbarButtons];
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self dismiss];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)url {
  if (url.gurl == GURL(kCBDSignOutOfChromeURL)) {
    [self showSignOutWithItemView:[view contentView]];
    return;
  }
  NSString* baseURL =
      [NSString stringWithCString:(url.gurl.host() + url.gurl.path()).c_str()
                         encoding:[NSString defaultCStringEncoding]];
  if ([[NSString stringWithCString:(kClearBrowsingDataDSESearchUrlInFooterURL)
                          encoding:[NSString defaultCStringEncoding]]
          rangeOfString:baseURL]
          .length > 0) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kSearchHistory);
  } else if ([[NSString stringWithCString:
                            (kClearBrowsingDataDSEMyActivityUrlInFooterURL)
                                 encoding:[NSString defaultCStringEncoding]]
                 rangeOfString:baseURL]
                 .length > 0) {
    base::UmaHistogramEnumeration("Settings.ClearBrowsingData.OpenMyActivity",
                                  MyActivityNavigation::kTopLevel);
  }
  [self.delegate openURL:url.gurl];
}

#pragma mark - ClearBrowsingDataConsumer

- (void)updateCellsForItem:(TableViewItem*)item reload:(BOOL)reload {
  if (self.suppressTableViewUpdates)
    return;

  if (!reload) {
    [self reconfigureCellsForItems:@[ item ]];
    NSIndexPath* indexPath = [self.tableViewModel
        indexPathForItem:static_cast<TableViewItem*>(item)];
    [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  } else {
    // Reload the item instead of reconfiguring it. This might update
    // TableViewLinkHeaderFooterView which which can have different number of
    // lines, thus the cell height needs to adapt accordingly.
    [self reloadCellsForItems:@[ item ]
             withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

- (void)removeBrowsingDataForBrowserState:(ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock {
  base::RecordAction(
      base::UserMetricsAction("MobileClearBrowsingDataTriggeredFromUIRefresh"));

  // Show activity indicator modal while removal is happening.
  self.overlayCoordinator = [[ChromeActivityOverlayCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:_browser];

  self.overlayCoordinator.messageText = l10n_util::GetNSStringWithFixup(
      IDS_IOS_CLEAR_BROWSING_DATA_ACTIVITY_MODAL);

  self.overlayCoordinator.blockAllWindows = YES;

  // Observe Gestures while overlay is visible to prevent user from swiping away
  // from this view during the process of clear browsing data.
  self.navigationController.interactivePopGestureRecognizer.delegate = self;
  [self.overlayCoordinator start];

  __weak ClearBrowsingDataTableViewController* weakSelf = self;
  dispatch_time_t timeOneSecondLater =
      dispatch_time(DISPATCH_TIME_NOW, (1 * NSEC_PER_SEC));
  void (^removeBrowsingDidFinishCompletionBlock)(void) = ^void() {
    ClearBrowsingDataTableViewController* strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }
    // Sometimes clear browsing data is really short
    // (<1sec), so ensure that overlay displays for at
    // least 1 second instead of looking like a glitch.
    dispatch_after(timeOneSecondLater, dispatch_get_main_queue(), ^{
      [self.overlayCoordinator stop];
      self.navigationController.interactivePopGestureRecognizer.delegate = nil;

      // Inform Voiceover users that their browsing data has
      // been cleared. Otherwise, users only hear that the clear browsing data
      // process was initiated, but not completed.
      UIAccessibilityPostNotification(
          UIAccessibilityAnnouncementNotification,
          l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE));

      if (completionBlock)
        completionBlock();
    });
  };

  // If browsing History will be cleared set the kLastClearBrowsingDataTime.
  // TODO(crbug.com/1085419): This pref is used by the Feed to prevent the
  // showing of customized content after history has been cleared. We might want
  // to create a specific Pref for this.
  if (IsRemoveDataMaskSet(removeMask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    browserState->GetPrefs()->SetInt64(
        browsing_data::prefs::kLastClearBrowsingDataTime,
        base::Time::Now().ToTimeT());

    DiscoverFeedServiceFactory::GetForBrowserState(browserState)
        ->BrowsingHistoryCleared();
  }

  [self.dispatcher
      removeBrowsingDataForBrowserState:browserState
                             timePeriod:timePeriod
                             removeMask:removeMask
                        completionBlock:removeBrowsingDidFinishCompletionBlock];
}

- (void)showBrowsingHistoryRemovedDialog {
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_DESCRIPTION);

  self.alertCoordinator =
      [[AlertCoordinator alloc] initWithBaseViewController:self
                                                   browser:_browser
                                                     title:title
                                                   message:message];

  __weak ClearBrowsingDataTableViewController* weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:
          l10n_util::GetNSString(
              IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OPEN_HISTORY_BUTTON)
                action:^{
                  [weakSelf.delegate openURL:GURL(kGoogleMyAccountURL)];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_CLEAR_BROWSING_DATA_HISTORY_NOTICE_OK_BUTTON)
                action:nil
                 style:UIAlertActionStyleCancel];

  [self.alertCoordinator start];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  base::RecordAction(
      base::UserMetricsAction("IOSClearBrowsingDataCloseWithSwipe"));
  // Call prepareForDismissal to clean up state and stop the Coordinator.
  [self prepareForDismissal];
}

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return !self.overlayCoordinator.started;
}

#pragma mark - SignoutActionSheetCoordinatorDelegate

- (void)signoutActionSheetCoordinatorPreventUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self preventUserInteraction];
}

- (void)signoutActionSheetCoordinatorAllowUserInteraction:
    (SignoutActionSheetCoordinator*)coordinator {
  [self allowUserInteraction];
}

#pragma mark - Private Helpers

- (void)showClearBrowsingDataAlertController:(id)sender {
  BrowsingDataRemoveMask dataTypeMaskToRemove =
      BrowsingDataRemoveMask::REMOVE_NOTHING;
  NSArray* dataTypeItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierDataTypes];
  for (TableViewClearBrowsingDataItem* dataTypeItem in dataTypeItems) {
    DCHECK([dataTypeItem isKindOfClass:[TableViewClearBrowsingDataItem class]]);
    if (dataTypeItem.checked) {
      dataTypeMaskToRemove = dataTypeMaskToRemove | dataTypeItem.dataTypeMask;
    }
  }
  self.actionSheetCoordinator = [self.dataManager
      actionSheetCoordinatorWithDataTypesToRemove:dataTypeMaskToRemove
                               baseViewController:self
                                          browser:_browser
                              sourceBarButtonItem:sender];
  [self.actionSheetCoordinator start];
}

- (void)updateToolbarButtons {
  self.clearBrowsingDataBarButton.enabled = [self hasDataTypeItemsSelected];
}

- (BOOL)hasDataTypeItemsSelected {
  // Returns YES iff at least 1 data type cell is selected.
  NSArray* dataTypeItems = [self.tableViewModel
      itemsInSectionWithIdentifier:SectionIdentifierDataTypes];
  for (TableViewClearBrowsingDataItem* dataTypeItem in dataTypeItems) {
    DCHECK([dataTypeItem isKindOfClass:[TableViewClearBrowsingDataItem class]]);
    if (dataTypeItem.checked) {
      return YES;
    }
  }
  return NO;
}

// Offer the user to sign-out near itemView
// If they sync, they can keep or delete their data.
// TODO(crbug.com/1385791) Test that correct histogram is registered.
- (void)showSignOutWithItemView:(UIView*)itemView {
  if (_signoutCoordinator) {
    // An action is already in progress, ignore user's request.
    return;
  }
  signin_metrics::ProfileSignout signout_source_metric = signin_metrics::
      ProfileSignout::kUserClickedSignoutFromClearBrowsingDataPage;
  _signoutCoordinator = [[SignoutActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                            rect:itemView.frame
                            view:itemView
                      withSource:signout_source_metric];
  _signoutCoordinator.showUnavailableFeatureDialogHeader = YES;
  __weak ClearBrowsingDataTableViewController* weakSelf = self;
  _signoutCoordinator.completion = ^(BOOL success) {
    [weakSelf handleAuthenticationOperationDidFinish];
  };
  _signoutCoordinator.delegate = self;
  [_signoutCoordinator start];
}

// Stops the signout coordinator.
- (void)handleAuthenticationOperationDidFinish {
  DCHECK(_signoutCoordinator);
  [_signoutCoordinator stop];
  _signoutCoordinator = nil;
}

@end
