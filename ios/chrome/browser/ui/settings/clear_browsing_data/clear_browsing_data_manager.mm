// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_manager.h"

#import "base/functional/bind.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/browsing_data/core/history_notice_utils.h"
#import "components/browsing_data/core/pref_names.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/google/core/common/google_util.h"
#import "components/history/core/browser/web_history_service.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/template_url_service.h"
#import "components/search_engines/template_url_service_observer.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/driver/sync_service.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browsing_data/browsing_data_counter_wrapper.h"
#import "ios/chrome/browser/browsing_data/browsing_data_features.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover_factory.h"
#import "ios/chrome/browser/browsing_data/browsing_data_remover_observer_bridge.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/history/web_history_service_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/settings/cells/clear_browsing_data_constants.h"
#import "ios/chrome/browser/ui/settings/cells/search_engine_item.h"
#import "ios/chrome/browser/ui/settings/cells/table_view_clear_browsing_data_item.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/browsing_data_counter_wrapper_producer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_consumer.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_constants.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/time_range_selector_table_view_controller.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

const char kCBDSignOutOfChromeURL[] = "settings://CBDSignOutOfChrome";

namespace {
// Maximum number of times to show a notice about other forms of browsing
// history.
const int kMaxTimesHistoryNoticeShown = 1;
// TableViewClearBrowsingDataItem's selectedBackgroundViewBackgroundColorAlpha.
const CGFloat kSelectedBackgroundColorAlpha = 0.05;

// The size of the symbol image used in the 'Clear Browsing Data' view.
const CGFloat kSymbolPointSize = 22;

// Returns the symbol coresponding to the given itemType.
UIImage* SymbolForItemType(ClearBrowsingDataItemType itemType) {
  UIImage* symbol = nil;
  switch (itemType) {
    case ItemTypeDataTypeBrowsingHistory:
      symbol =
          DefaultSymbolTemplateWithPointSize(kHistorySymbol, kSymbolPointSize);
      break;
    case ItemTypeDataTypeCookiesSiteData:
      symbol = DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                                  kSymbolPointSize);
      break;
    case ItemTypeDataTypeSavedPasswords:
      symbol =
          CustomSymbolTemplateWithPointSize(kPasswordSymbol, kSymbolPointSize);
      break;
    case ItemTypeDataTypeCache:
      symbol = DefaultSymbolTemplateWithPointSize(kCachedDataSymbol,
                                                  kSymbolPointSize);
      break;
    case ItemTypeDataTypeAutofill:
      symbol = DefaultSymbolTemplateWithPointSize(kAutofillDataSymbol,
                                                  kSymbolPointSize);
      break;
    default:
      NOTREACHED();
      break;
  }
  return symbol;
}

}  // namespace

@interface ClearBrowsingDataManager () <BrowsingDataRemoverObserving,
                                        PrefObserverDelegate> {
  // Access to the kDeleteTimePeriod preference.
  IntegerPrefMember _timeRangePref;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // Observer for browsing data removal events and associated
  // base::ScopedObservation used to track registration with
  // BrowsingDataRemover.
  std::unique_ptr<BrowsingDataRemoverObserver> _browsingDataRemoverObserver;
  std::unique_ptr<
      base::ScopedObservation<BrowsingDataRemover, BrowsingDataRemoverObserver>>
      _scoped_observation;

  // Corresponds browsing data counters to their masks/flags. Items are inserted
  // as clear data items are constructed.
  std::map<BrowsingDataRemoveMask, std::unique_ptr<BrowsingDataCounterWrapper>>
      _countersByMasks;
}

@property(nonatomic, assign) ChromeBrowserState* browserState;
// Whether to show alert about other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
// Whether to show popup other forms of browsing history.
@property(nonatomic, assign)
    BOOL shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

@property(nonatomic, strong) TableViewDetailIconItem* tableViewTimeRangeItem;

@property(nonatomic, strong)
    TableViewClearBrowsingDataItem* browsingHistoryItem;
@property(nonatomic, strong)
    TableViewClearBrowsingDataItem* cookiesSiteDataItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* cacheItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* savedPasswordsItem;
@property(nonatomic, strong) TableViewClearBrowsingDataItem* autofillItem;

@property(nonatomic, strong)
    BrowsingDataCounterWrapperProducer* counterWrapperProducer;

@end

@implementation ClearBrowsingDataManager
@synthesize browserState = _browserState;
@synthesize consumer = _consumer;
@synthesize shouldShowNoticeAboutOtherFormsOfBrowsingHistory =
    _shouldShowNoticeAboutOtherFormsOfBrowsingHistory;
@synthesize shouldPopupDialogAboutOtherFormsOfBrowsingHistory =
    _shouldPopupDialogAboutOtherFormsOfBrowsingHistory;

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState {
  return [self initWithBrowserState:browserState
                     browsingDataRemover:BrowsingDataRemoverFactory::
                                             GetForBrowserState(browserState)
      browsingDataCounterWrapperProducer:[[BrowsingDataCounterWrapperProducer
                                             alloc] init]];
}

- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                   browsingDataRemover:(BrowsingDataRemover*)remover
    browsingDataCounterWrapperProducer:
        (BrowsingDataCounterWrapperProducer*)producer {
  self = [super init];
  if (self) {
    _browserState = browserState;
    _counterWrapperProducer = producer;

    _timeRangePref.Init(browsing_data::prefs::kDeleteTimePeriod,
                        _browserState->GetPrefs());

    _browsingDataRemoverObserver =
        std::make_unique<BrowsingDataRemoverObserverBridge>(self);
    _scoped_observation =
        std::make_unique<base::ScopedObservation<BrowsingDataRemover,
                                                 BrowsingDataRemoverObserver>>(
            _browsingDataRemoverObserver.get());
    _scoped_observation->Observe(remover);

    _prefChangeRegistrar.Init(_browserState->GetPrefs());
    _prefObserverBridge.reset(new PrefObserverBridge(self));
  }
  return self;
}

#pragma mark - Public Methods

- (void)loadModel:(ListModel*)model {
  self.tableViewTimeRangeItem = [self timeRangeItem];

  [model addSectionWithIdentifier:SectionIdentifierTimeRange];
  [model addItem:self.tableViewTimeRangeItem
      toSectionWithIdentifier:SectionIdentifierTimeRange];
  [self addClearBrowsingDataItemsToModel:model];
  [self addSyncProfileItemsToModel:model];
}

- (void)updateModel:(ListModel*)model withTableView:(UITableView*)tableView {
  const BOOL hasSectionSavedSiteData =
      [model hasSectionForSectionIdentifier:SectionIdentifierSavedSiteData];
  if (hasSectionSavedSiteData == [self loggedIn]) {
    // Nothing to do. We have data iff we are logged-in
    return;
  }
  if (hasSectionSavedSiteData) {
    // User signed-out, no need for footer anymore.
    [model removeSectionWithIdentifier:SectionIdentifierSavedSiteData];
  } else if (!hasSectionSavedSiteData) {
    // User signed-in, we need to add footer
    [self addSavedSiteDataSectionWithModel:model];
  }
  [tableView reloadData];
}

- (void)prepare {
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteTimePeriod, &_prefChangeRegistrar);

  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteBrowsingHistory, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteCookies, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteCache, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeletePasswords, &_prefChangeRegistrar);
  _prefObserverBridge->ObserveChangesForPreference(
      browsing_data::prefs::kDeleteFormData, &_prefChangeRegistrar);
}

- (void)disconnect {
  _timeRangePref.Destroy();
  _prefObserverBridge.reset();
  _prefChangeRegistrar.RemoveAll();
  _browsingDataRemoverObserver.reset();
  _scoped_observation.reset();
  _countersByMasks.clear();
}

// Add items for types of browsing data to clear.
- (void)addClearBrowsingDataItemsToModel:(ListModel*)model {
  // Data types section.
  [model addSectionWithIdentifier:SectionIdentifierDataTypes];
  self.browsingHistoryItem =
      [self clearDataItemWithType:ItemTypeDataTypeBrowsingHistory
                          titleID:IDS_IOS_CLEAR_BROWSING_HISTORY
                             mask:BrowsingDataRemoveMask::REMOVE_HISTORY
                         prefName:browsing_data::prefs::kDeleteBrowsingHistory];
  [model addItem:self.browsingHistoryItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  // This data type doesn't currently have an associated counter, but displays
  // an explanatory text instead.
  self.cookiesSiteDataItem =
      [self clearDataItemWithType:ItemTypeDataTypeCookiesSiteData
                          titleID:IDS_IOS_CLEAR_COOKIES
                             mask:BrowsingDataRemoveMask::REMOVE_SITE_DATA
                         prefName:browsing_data::prefs::kDeleteCookies];
  [model addItem:self.cookiesSiteDataItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.cacheItem =
      [self clearDataItemWithType:ItemTypeDataTypeCache
                          titleID:IDS_IOS_CLEAR_CACHE
                             mask:BrowsingDataRemoveMask::REMOVE_CACHE
                         prefName:browsing_data::prefs::kDeleteCache];
  [model addItem:self.cacheItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.savedPasswordsItem =
      [self clearDataItemWithType:ItemTypeDataTypeSavedPasswords
                          titleID:IDS_IOS_CLEAR_SAVED_PASSWORDS
                             mask:BrowsingDataRemoveMask::REMOVE_PASSWORDS
                         prefName:browsing_data::prefs::kDeletePasswords];
  [model addItem:self.savedPasswordsItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];

  self.autofillItem =
      [self clearDataItemWithType:ItemTypeDataTypeAutofill
                          titleID:IDS_IOS_CLEAR_AUTOFILL
                             mask:BrowsingDataRemoveMask::REMOVE_FORM_DATA
                         prefName:browsing_data::prefs::kDeleteFormData];
  [model addItem:self.autofillItem
      toSectionWithIdentifier:SectionIdentifierDataTypes];
}

- (NSString*)counterTextFromResult:
    (const browsing_data::BrowsingDataCounter::Result&)result {
  if (!result.Finished()) {
    // The counter is still counting.
    return l10n_util::GetNSString(IDS_CLEAR_BROWSING_DATA_CALCULATING);
  }

  base::StringPiece prefName = result.source()->GetPrefName();
  if (prefName != browsing_data::prefs::kDeleteCache) {
    return base::SysUTF16ToNSString(
        browsing_data::GetCounterTextFromResult(&result));
  }

  browsing_data::BrowsingDataCounter::ResultInt cacheSizeBytes =
      static_cast<const browsing_data::BrowsingDataCounter::FinishedResult*>(
          &result)
          ->Value();

  // Three cases: Nonzero result for the entire cache, nonzero result for
  // a subset of cache (i.e. a finite time interval), and almost zero (less
  // than 1 MB). There is no exact information that the cache is empty so that
  // falls into the almost zero case, which is displayed as less than 1 MB.
  // Because of this, the lowest unit that can be used is MB.
  static const int kBytesInAMegabyte = 1 << 20;
  if (cacheSizeBytes >= kBytesInAMegabyte) {
    NSByteCountFormatter* formatter = [[NSByteCountFormatter alloc] init];
    formatter.allowedUnits = NSByteCountFormatterUseAll &
                             (~NSByteCountFormatterUseBytes) &
                             (~NSByteCountFormatterUseKB);
    formatter.countStyle = NSByteCountFormatterCountStyleMemory;
    NSString* formattedSize = [formatter stringFromByteCount:cacheSizeBytes];
    return _timeRangePref.GetValue() ==
                   static_cast<int>(browsing_data::TimePeriod::ALL_TIME)
               ? formattedSize
               : l10n_util::GetNSStringF(
                     IDS_DEL_CACHE_COUNTER_UPPER_ESTIMATE,
                     base::SysNSStringToUTF16(formattedSize));
  }

  return l10n_util::GetNSString(IDS_DEL_CACHE_COUNTER_ALMOST_EMPTY);
}

- (ActionSheetCoordinator*)
    actionSheetCoordinatorWithDataTypesToRemove:
        (BrowsingDataRemoveMask)dataTypeMaskToRemove
                             baseViewController:
                                 (UIViewController*)baseViewController
                                        browser:(Browser*)browser
                            sourceBarButtonItem:
                                (UIBarButtonItem*)sourceBarButtonItem {
  if (dataTypeMaskToRemove == BrowsingDataRemoveMask::REMOVE_NOTHING) {
    // Nothing to clear (no data types selected).
    return nil;
  }
  __weak ClearBrowsingDataManager* weakSelf = self;

  ActionSheetCoordinator* actionCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:baseViewController
                         browser:browser
                           title:l10n_util::GetNSString(
                                     IDS_IOS_CONFIRM_CLEAR_BUTTON_TITLE)
                         message:nil
                   barButtonItem:sourceBarButtonItem];

  // For larger texts, use `UIAlertControllerStyleAlert` for the alert's style
  // as it gracefully handles text overflow by adding scrollbars to content.
  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    actionCoordinator.alertStyle = UIAlertControllerStyleAlert;
  }

  actionCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionDown | UIPopoverArrowDirectionUp;
  [actionCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_IOS_CLEAR_BUTTON)
                action:^{
                  [weakSelf clearDataForDataTypes:dataTypeMaskToRemove];
                }
                 style:UIAlertActionStyleDestructive];
  return actionCoordinator;
}

// Add footers about user's account data.
- (void)addSyncProfileItemsToModel:(ListModel*)model {
  // Google Account footer.
  const BOOL loggedIn = [self loggedIn];
  const TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(_browserState);
  const TemplateURL* defaultSearchEngine =
      templateURLService->GetDefaultSearchProvider();
  const BOOL isDefaultSearchEngineGoogle =
      defaultSearchEngine->GetEngineType(
          templateURLService->search_terms_data()) ==
      SearchEngineType::SEARCH_ENGINE_GOOGLE;
  // If the user has their DSE set to Google and is logged out
  // there is no additional data to delete, so omit this section.
  if (isDefaultSearchEngineGoogle && !loggedIn) {
    // Nothing to do.
  } else {
    // Show additional instructions for deleting data.
    [model addSectionWithIdentifier:SectionIdentifierGoogleAccount];
    [model setFooter:[self footerGoogleAccountDSEBasedItem:loggedIn
                                       defaultSearchEngine:defaultSearchEngine
                               isDefaultSearchEngineGoogle:
                                   isDefaultSearchEngineGoogle]
        forSectionWithIdentifier:SectionIdentifierGoogleAccount];
  }

  syncer::SyncService* syncService = [self syncService];
  [self addSavedSiteDataSectionWithModel:model];

  // If not syncing, no need to continue with profile syncing.
  if (![self identityManager]->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  history::WebHistoryService* historyService =
      ios::WebHistoryServiceFactory::GetForBrowserState(_browserState);

  __weak ClearBrowsingDataManager* weakSelf = self;

  browsing_data::ShouldPopupDialogAboutOtherFormsOfBrowsingHistory(
      syncService, historyService, GetChannel(),
      base::BindOnce(^(bool shouldShowPopup) {
        ClearBrowsingDataManager* strongSelf = weakSelf;
        [strongSelf setShouldPopupDialogAboutOtherFormsOfBrowsingHistory:
                        shouldShowPopup];
      }));
}

- (void)restartCounters:(BrowsingDataRemoveMask)mask {
  // List of flags that have corresponding counters.
  static const BrowsingDataRemoveMask browsingDataRemoveFlags[] = {
      // BrowsingDataRemoveMask::REMOVE_COOKIES not included; we don't have
      // cookie counters yet.
      BrowsingDataRemoveMask::REMOVE_HISTORY,
      BrowsingDataRemoveMask::REMOVE_CACHE,
      BrowsingDataRemoveMask::REMOVE_PASSWORDS,
      BrowsingDataRemoveMask::REMOVE_FORM_DATA,
  };

  for (auto flag : browsingDataRemoveFlags) {
    if (IsRemoveDataMaskSet(mask, flag)) {
      const auto it = _countersByMasks.find(flag);
      if (it != _countersByMasks.end()) {
        it->second->RestartCounter();
      }
    }
  }
}

#pragma mark Items

// Creates item of type `itemType` with `mask` of data to be cleared if
// selected, `prefName`, and `titleId` of item.
- (TableViewClearBrowsingDataItem*)
    clearDataItemWithType:(ClearBrowsingDataItemType)itemType
                  titleID:(int)titleMessageID
                     mask:(BrowsingDataRemoveMask)mask
                 prefName:(const char*)prefName {
  PrefService* prefs = self.browserState->GetPrefs();
  TableViewClearBrowsingDataItem* clearDataItem =
      [[TableViewClearBrowsingDataItem alloc] initWithType:itemType];
  clearDataItem.text = l10n_util::GetNSString(titleMessageID);
  clearDataItem.checked = prefs->GetBoolean(prefName);
  clearDataItem.accessibilityIdentifier =
      [self accessibilityIdentifierFromItemType:itemType];
  clearDataItem.dataTypeMask = mask;
  clearDataItem.prefName = prefName;
  clearDataItem.checkedBackgroundColor = [[UIColor colorNamed:kBlueColor]
      colorWithAlphaComponent:kSelectedBackgroundColorAlpha];

  clearDataItem.image = SymbolForItemType(itemType);

  if (itemType == ItemTypeDataTypeCookiesSiteData) {
    // Because there is no counter for cookies, an explanatory text is
    // displayed.
    clearDataItem.detailText = l10n_util::GetNSString(IDS_DEL_COOKIES_COUNTER);
  } else {
    // Having a placeholder `detailText` helps reduce the observable
    // row-height changes induced by the counter callbacks.
    clearDataItem.detailText = @"\u00A0";
    __weak ClearBrowsingDataManager* weakSelf = self;
    __weak TableViewClearBrowsingDataItem* weakTableClearDataItem =
        clearDataItem;
    BrowsingDataCounterWrapper::UpdateUICallback callback = base::BindRepeating(
        ^(const browsing_data::BrowsingDataCounter::Result& result) {
          weakTableClearDataItem.detailText =
              [weakSelf counterTextFromResult:result];
          [weakSelf.consumer updateCellsForItem:weakTableClearDataItem
                                         reload:YES];
        });
    std::unique_ptr<BrowsingDataCounterWrapper> counter =
        [self.counterWrapperProducer
            createCounterWrapperWithPrefName:prefName
                                browserState:self.browserState
                                 prefService:prefs
                            updateUiCallback:callback];
    _countersByMasks.emplace(mask, std::move(counter));
  }
  return clearDataItem;
}

- (TableViewLinkHeaderFooterItem*)footerForGoogleAccountSectionItem {
  return _shouldShowNoticeAboutOtherFormsOfBrowsingHistory
             ? [self footerGoogleAccountAndMyActivityItem]
             : [self footerGoogleAccountItem];
}

- (TableViewLinkHeaderFooterItem*)
    footerGoogleAccountDSEBasedItem:(const BOOL)loggedIn
                defaultSearchEngine:(const TemplateURL*)defaultSearchEngine
        isDefaultSearchEngineGoogle:(const BOOL)isDefaultSearchEngineGoogle {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeFooterGoogleAccountDSEBased];
  if (loggedIn) {
    if (isDefaultSearchEngineGoogle) {
      footerItem.text =
          l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_GOOGLE_DSE);
      footerItem.urls = @[
        [[CrURL alloc]
            initWithGURL:google_util::AppendGoogleLocaleParam(
                             GURL(kClearBrowsingDataDSESearchUrlInFooterURL),
                             GetApplicationContext()->GetApplicationLocale())],
        [[CrURL alloc]
            initWithGURL:google_util::AppendGoogleLocaleParam(
                             GURL(
                                 kClearBrowsingDataDSEMyActivityUrlInFooterURL),
                             GetApplicationContext()->GetApplicationLocale())]
      ];
    } else if (defaultSearchEngine->prepopulate_id() > 0) {
      footerItem.text = l10n_util::GetNSStringF(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_KNOWN_DSE_SIGNED_IN,
          defaultSearchEngine->short_name());
      footerItem.urls = @[ [[CrURL alloc]
          initWithGURL:google_util::AppendGoogleLocaleParam(
                           GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL),
                           GetApplicationContext()->GetApplicationLocale())] ];
    } else {
      footerItem.text = l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_UNKOWN_DSE_SIGNED_IN);
      footerItem.urls = @[ [[CrURL alloc]
          initWithGURL:google_util::AppendGoogleLocaleParam(
                           GURL(kClearBrowsingDataDSEMyActivityUrlInFooterURL),
                           GetApplicationContext()->GetApplicationLocale())] ];
    }
  } else {
    // Logged Out with Google DSE is handled in calling function since there
    // should be no account footer section in this case.
    if (defaultSearchEngine->prepopulate_id() > 0) {
      footerItem.text = l10n_util::GetNSStringF(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_KNOWN_DSE_SIGNED_OUT,
          defaultSearchEngine->short_name());
    } else {
      footerItem.text = l10n_util::GetNSString(
          IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_UNKOWN_DSE_SIGNED_OUT);
    }
  }
  return footerItem;
}

- (TableViewLinkHeaderFooterItem*)footerGoogleAccountItem {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ItemTypeFooterGoogleAccount];
  footerItem.text =
      l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT);
  return footerItem;
}

- (TableViewLinkHeaderFooterItem*)footerGoogleAccountAndMyActivityItem {
  return [self
      footerItemWithType:ItemTypeFooterGoogleAccountAndMyActivity
                 titleID:IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_ACCOUNT_AND_HISTORY
                     URL:kClearBrowsingDataMyActivityUrlInFooterURL
       appendLocaleToURL:YES];
}

- (TableViewLinkHeaderFooterItem*)signOutFooterItem {
  return [self
      footerItemWithType:ItemTypeFooterSignoutOfGoogle
                 titleID:
                     IDS_IOS_CLEAR_BROWSING_DATA_FOOTER_SIGN_OUT_EVERY_WEBSITE
                     URL:kCBDSignOutOfChromeURL
       appendLocaleToURL:NO];
}

// Creates item of type `itemType` with `titleMessageId`, containing a link to
// `URL`. If appendLocaleToURL, the local is added to the URL.
- (TableViewLinkHeaderFooterItem*)footerItemWithType:
                                      (ClearBrowsingDataItemType)itemType
                                             titleID:(int)titleMessageID
                                                 URL:(const char[])URL
                                   appendLocaleToURL:(BOOL)appendLocaleToURL {
  TableViewLinkHeaderFooterItem* footerItem =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:itemType];
  footerItem.text = l10n_util::GetNSString(titleMessageID);
  GURL gurl = GURL(URL);
  if (appendLocaleToURL) {
    gurl = google_util::AppendGoogleLocaleParam(
        gurl, GetApplicationContext()->GetApplicationLocale());
  }
  footerItem.urls = @[ [[CrURL alloc] initWithGURL:gurl] ];
  return footerItem;
}

- (TableViewDetailIconItem*)timeRangeItem {
  TableViewDetailIconItem* timeRangeItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeTimeRange];
  timeRangeItem.text = l10n_util::GetNSString(
      IDS_IOS_CLEAR_BROWSING_DATA_TIME_RANGE_SELECTOR_TITLE);
  NSString* detailText = [TimeRangeSelectorTableViewController
      timePeriodLabelForPrefs:self.browserState->GetPrefs()];
  DCHECK(detailText);
  timeRangeItem.detailText = detailText;
  timeRangeItem.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  timeRangeItem.accessibilityTraits |= UIAccessibilityTraitButton;
  return timeRangeItem;
}

- (NSString*)accessibilityIdentifierFromItemType:(NSInteger)itemType {
  switch (itemType) {
    case ItemTypeDataTypeBrowsingHistory:
      return kClearBrowsingHistoryCellAccessibilityIdentifier;
    case ItemTypeDataTypeCookiesSiteData:
      return kClearCookiesCellAccessibilityIdentifier;
    case ItemTypeDataTypeCache:
      return kClearCacheCellAccessibilityIdentifier;
    case ItemTypeDataTypeSavedPasswords:
      return kClearSavedPasswordsCellAccessibilityIdentifier;
    case ItemTypeDataTypeAutofill:
      return kClearAutofillCellAccessibilityIdentifier;
    default: {
      NOTREACHED();
      return nil;
    }
  }
}

#pragma mark - Private Methods

// An identity manager
- (signin::IdentityManager*)identityManager {
  return IdentityManagerFactory::GetForBrowserState(self.browserState);
}

// Whether user is currently logged-in.
- (BOOL)loggedIn {
  return
      [self identityManager]->HasPrimaryAccount(signin::ConsentLevel::kSignin);
}

// A sync service
- (syncer::SyncService*)syncService {
  return SyncServiceFactory::GetForBrowserState(self.browserState);
}

// Add at the end of the list model the elements related to signing-out.
- (void)addSavedSiteDataSectionWithModel:(ListModel*)model {
  if ([self loggedIn]) {
    [model addSectionWithIdentifier:SectionIdentifierSavedSiteData];
    [model setFooter:[self signOutFooterItem]
        forSectionWithIdentifier:SectionIdentifierSavedSiteData];
  }
}

- (void)clearDataForDataTypes:(BrowsingDataRemoveMask)mask {
  DCHECK(mask != BrowsingDataRemoveMask::REMOVE_NOTHING);

  browsing_data::TimePeriod timePeriod =
      static_cast<browsing_data::TimePeriod>(_timeRangePref.GetValue());
  [self.consumer removeBrowsingDataForBrowserState:_browserState
                                        timePeriod:timePeriod
                                        removeMask:mask
                                   completionBlock:nil];

  // Send the "Cleared Browsing Data" event to the feature_engagement::Tracker
  // when the user initiates a clear browsing data action. No event is sent if
  // the browsing data is cleared without the user's input.
  feature_engagement::TrackerFactory::GetForBrowserState(_browserState)
      ->NotifyEvent(feature_engagement::events::kClearedBrowsingData);

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    PrefService* prefs = _browserState->GetPrefs();
    int noticeShownTimes = prefs->GetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes);

    // When the deletion is complete, we might show an additional dialog with
    // a notice about other forms of browsing history. This is the case if
    const bool showDialog =
        // 1. The dialog is relevant for the user.
        _shouldPopupDialogAboutOtherFormsOfBrowsingHistory &&
        // 2. The notice has been shown less than `kMaxTimesHistoryNoticeShown`.
        noticeShownTimes < kMaxTimesHistoryNoticeShown;
    if (!showDialog) {
      return;
    }
    UMA_HISTOGRAM_BOOLEAN(
        "History.ClearBrowsingData.ShownHistoryNoticeAfterClearing",
        showDialog);

    // Increment the preference.
    prefs->SetInteger(
        browsing_data::prefs::kClearBrowsingDataHistoryNoticeShownTimes,
        noticeShownTimes + 1);
    [self.consumer showBrowsingHistoryRemovedDialog];
  }
}

#pragma mark Properties

- (void)setShouldShowNoticeAboutOtherFormsOfBrowsingHistory:(BOOL)showNotice
                                                   forModel:(ListModel*)model {
  _shouldShowNoticeAboutOtherFormsOfBrowsingHistory = showNotice;
  // Update the account footer if the model was already loaded.
  if (!model) {
    return;
  }
  UMA_HISTOGRAM_BOOLEAN(
      "History.ClearBrowsingData.HistoryNoticeShownInFooterWhenUpdated",
      _shouldShowNoticeAboutOtherFormsOfBrowsingHistory);

  if (![self identityManager]->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    return;
  }

  [model setFooter:[self footerForGoogleAccountSectionItem]
      forSectionWithIdentifier:SectionIdentifierGoogleAccount];
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  PrefService* prefs = self.browserState->GetPrefs();
  if (preferenceName == browsing_data::prefs::kDeleteTimePeriod) {
    NSString* detailText =
        [TimeRangeSelectorTableViewController timePeriodLabelForPrefs:prefs];
    self.tableViewTimeRangeItem.detailText = detailText;
    [self.consumer updateCellsForItem:self.tableViewTimeRangeItem reload:YES];
  } else if (preferenceName == browsing_data::prefs::kDeleteBrowsingHistory) {
    self.browsingHistoryItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.browsingHistoryItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteCookies) {
    self.cookiesSiteDataItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.cookiesSiteDataItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteCache) {
    self.cacheItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.cacheItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeletePasswords) {
    self.savedPasswordsItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.savedPasswordsItem reload:NO];
  } else if (preferenceName == browsing_data::prefs::kDeleteFormData) {
    self.autofillItem.checked = prefs->GetBoolean(preferenceName);
    [self.consumer updateCellsForItem:self.autofillItem reload:NO];
  } else {
    DCHECK(false) << "Unxpected clear browsing data item type.";
  }
}

#pragma mark BrowsingDataRemoverObserving

- (void)browsingDataRemover:(BrowsingDataRemover*)remover
    didRemoveBrowsingDataWithMask:(BrowsingDataRemoveMask)mask {
  [self restartCounters:mask];
}

@end
