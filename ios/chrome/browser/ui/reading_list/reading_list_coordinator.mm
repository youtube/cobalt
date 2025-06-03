// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reading_list/reading_list_coordinator.h"

#import "base/ios/ios_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/prefs/pref_service.h"
#import "components/reading_list/core/reading_list_entry.h"
#import "components/reading_list/features/reading_list_switches.h"
#import "components/signin/public/base/signin_pref_names.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "components/sync/base/features.h"
#import "components/sync/base/user_selectable_type.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/reading_list/model/offline_page_tab_helper.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller_constants.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_consumer.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/authentication/signin_presenter.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/incognito_reauth/incognito_reauth_scene_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_histograms.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_item_factory_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_audience.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_list_view_controller_delegate.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_mediator.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_menu_provider.h"
#import "ios/chrome/browser/ui/reading_list/reading_list_table_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/features.h"
#import "ios/web/public/navigation/referrer.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"
#import "url/gurl.h"

// TODO(crbug.com/1425862): SigninPromoViewMediator will be refactored so that
// we can move the SigninPromoViewConsumer implementation from the coordinator
// to the view.
@interface ReadingListCoordinator () <IdentityManagerObserverBridgeDelegate,
                                      ReadingListMenuProvider,
                                      ReadingListListItemFactoryDelegate,
                                      ReadingListListViewControllerAudience,
                                      ReadingListListViewControllerDelegate,
                                      SigninPresenter,
                                      SigninPromoViewConsumer>

// Whether the coordinator is started.
@property(nonatomic, assign, getter=isStarted) BOOL started;
// The mediator that updates the table for model changes.
@property(nonatomic, strong) ReadingListMediator* mediator;
// The navigation controller displaying self.tableViewController.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;
// The view controller used to display the reading list.
@property(nonatomic, strong)
    ReadingListTableViewController* tableViewController;
// Coordinator in charge of handling sharing use cases.
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

@end

@implementation ReadingListCoordinator {
  // Observer for changes to the user's Google identities.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityManagerObserverBridge;
  // Whether the sign-in promo is shown or not.
  BOOL _shouldShowSignInPromo;
  // The mediator that updates the sign-in promo view.
  SigninPromoViewMediator* _signinPromoViewMediator;
  // Handler for sign-in commands.
  id<ApplicationCommands> _applicationCommandsHandler;
  // Authentication Service to retrieve the user's signed-in state.
  AuthenticationService* _authService;
  // Service to retrieve preference values.
  PrefService* _prefService;
  // Manager for user's Google identities.
  signin::IdentityManager* _identityManager;
  // Sync service.
  syncer::SyncService* _syncService;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (self.started)
    return;

  // Similar to the bookmarks, the content and sign-in promo state should remain
  // the same in the incognito mode.
  ChromeBrowserState* browserState =
      self.browser->GetBrowserState()->GetOriginalChromeBrowserState();

  // Create the mediator.
  ReadingListModel* model =
      ReadingListModelFactory::GetInstance()->GetForBrowserState(browserState);
  _syncService = SyncServiceFactory::GetForBrowserState(browserState);
  ReadingListListItemFactory* itemFactory =
      [[ReadingListListItemFactory alloc] init];
  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForBrowserState(browserState);
  self.mediator = [[ReadingListMediator alloc] initWithModel:model
                                                 syncService:_syncService
                                               faviconLoader:faviconLoader
                                             listItemFactory:itemFactory];
  // Initialize services.
  _applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  _authService = AuthenticationServiceFactory::GetForBrowserState(browserState);
  _identityManager = IdentityManagerFactory::GetForBrowserState(browserState);
  _prefService = browserState->GetPrefs();

  // Create the table.
  self.tableViewController = [[ReadingListTableViewController alloc] init];
  // Browser needs to be set before dataSource since the latter triggers a
  // reloadData call.
  self.tableViewController.browser = self.browser;
  self.tableViewController.delegate = self;
  self.tableViewController.audience = self;
  self.tableViewController.dataSource = self.mediator;

  self.tableViewController.menuProvider = self;

  itemFactory.accessibilityDelegate = self.tableViewController;

  // Add the "Done" button and hook it up to `stop`.
  UIBarButtonItem* dismissButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(dismissButtonTapped)];
  [dismissButton
      setAccessibilityIdentifier:kTableViewNavigationDismissButtonId];
  self.tableViewController.navigationItem.rightBarButtonItem = dismissButton;

  // Present RecentTabsNavigationController.
  self.navigationController = [[TableViewNavigationController alloc]
      initWithTable:self.tableViewController];

  // The initial call to `readingListHasItems:` may have been received before
  // all UI elements were initialized. Call the callback directly to set up
  // everything correctly.
  [self readingListHasItems:self.mediator.hasElements];

  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.presentationController.delegate =
      self.tableViewController;

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  // Send the "Viewed Reading List" event to the feature_engagement::Tracker
  // when the user opens their reading list.
  feature_engagement::TrackerFactory::GetForBrowserState(browserState)
      ->NotifyEvent(feature_engagement::events::kViewedReadingList);

  // Create the sign-in promo view mediator.
  _identityManagerObserverBridge.reset(
      new signin::IdentityManagerObserverBridge(_identityManager, self));
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(browserState);
  _signinPromoViewMediator = [[SigninPromoViewMediator alloc]
      initWithAccountManagerService:accountManagerService
                        authService:_authService
                        prefService:_prefService
                        syncService:_syncService
                        accessPoint:signin_metrics::AccessPoint::
                                        ACCESS_POINT_READING_LIST
                          presenter:self];
  _signinPromoViewMediator.signinPromoAction =
      SigninPromoAction::kInstantSignin;
  _signinPromoViewMediator.consumer = self;
  _signinPromoViewMediator.dataTypeToWaitForInitialSync =
      syncer::ModelType::READING_LIST;
  [self updateSignInPromoVisibility];

  [super start];
  self.started = YES;
}

- (void)dismissButtonTapped {
  base::RecordAction(base::UserMetricsAction("MobileReadingListClose"));
  [_delegate closeReadingList];
}

- (void)stop {
  if (!self.started)
    return;
  [self.tableViewController willBeDismissed];
  [self.navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.tableViewController = nil;
  // It is possible that the user opens the reading list when there's already
  // a reading list view (with tap on the NTP icons for instance when the
  // previous reading list is dismissing).
  // In this case, `closeReadingList` (thus this `stop` method) is called
  // immediately, and `presentationController.delegate` needs be set to nil to
  // avoid receiving `presentationControllerDidDismiss` which calls
  // `closeReadingList` again.
  // See https://crbug.com/1449105.
  self.navigationController.presentationController.delegate = nil;
  self.navigationController = nil;

  [self.mediator disconnect];
  self.mediator = nil;

  [self.sharingCoordinator stop];
  self.sharingCoordinator = nil;

  [_signinPromoViewMediator disconnect];
  _signinPromoViewMediator = nil;

  _authService = nullptr;
  _prefService = nullptr;
  _identityManager = nullptr;
  _syncService = nullptr;
  _identityManagerObserverBridge.reset();

  [super stop];
  self.started = NO;
}

- (void)dealloc {
  DCHECK(!self.mediator);
}

#pragma mark - ReadingListListViewControllerAudience

- (void)readingListHasItems:(BOOL)hasItems {
  self.navigationController.toolbarHidden = !hasItems;
}

#pragma mark - ReadingListTableViewControllerDelegate

- (void)dismissReadingListListViewController:(UIViewController*)viewController {
  DCHECK_EQ(self.tableViewController, viewController);
  [self.tableViewController willBeDismissed];
  [_delegate closeReadingList];
}

- (void)readingListListViewController:(UIViewController*)viewController
                             openItem:(id<ReadingListListItem>)item {
  DCHECK_EQ(self.tableViewController, viewController);
  scoped_refptr<const ReadingListEntry> entry =
      [self.mediator entryFromItem:item];
  if (!entry) {
    [self.tableViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
          withOfflineURL:GURL::EmptyGURL()
      loadOfflineVersion:NO
                inNewTab:NO
               incognito:NO];
}

- (void)readingListListViewController:(UIViewController*)viewController
                     openItemInNewTab:(id<ReadingListListItem>)item
                            incognito:(BOOL)incognito {
  DCHECK_EQ(self.tableViewController, viewController);
  scoped_refptr<const ReadingListEntry> entry =
      [self.mediator entryFromItem:item];
  if (!entry) {
    [self.tableViewController reloadData];
    return;
  }
  [self loadEntryURL:entry->URL()
          withOfflineURL:GURL::EmptyGURL()
      loadOfflineVersion:NO
                inNewTab:YES
               incognito:incognito];
}

- (void)readingListListViewController:(UIViewController*)viewController
              openItemOfflineInNewTab:(id<ReadingListListItem>)item {
  DCHECK_EQ(self.tableViewController, viewController);
  [self openItemOfflineInNewTab:item];
}

- (void)didLoadContent {
  if (!_shouldShowSignInPromo) {
    return;
  }

  SigninPromoViewConfigurator* promoConfigurator =
      [_signinPromoViewMediator createConfigurator];
  [self.tableViewController promoStateChanged:YES
                            promoConfigurator:promoConfigurator
                                promoDelegate:_signinPromoViewMediator];
}

#pragma mark - URL Loading Helpers

// Loads reading list URLs. If `offlineURL` is valid and `loadOfflineVersion` is
// true, the item will be loaded offline; otherwise `entryURL` is loaded.
// `newTab` and `incognito` can be used to optionally open the URL in a new tab
// or in incognito.  The coordinator is also stopped after the load is
// requested.
// NOTE: `loadOfflineVersion` may not be used with `inNewTab`.
// TODO(crbug.com/1313458):  Remove `inNewTab` and `withOfflineURL` when
// migration is complete.
- (void)loadEntryURL:(const GURL&)entryURL
        withOfflineURL:(const GURL&)offlineURL
    loadOfflineVersion:(BOOL)loadOfflineVersion
              inNewTab:(BOOL)newTab
             incognito:(BOOL)incognito {
  // Override incognito opening using enterprise policy.
  incognito = incognito || self.isIncognitoForced;
  incognito = incognito && self.isIncognitoAvailable;
  // Only open a new incognito tab when incognito is authenticated. Prompt for
  // auth otherwise.
  if (incognito) {
    IncognitoReauthSceneAgent* reauthAgent = [IncognitoReauthSceneAgent
        agentFromScene:SceneStateBrowserAgent::FromBrowser(self.browser)
                           ->GetSceneState()];
    __weak ReadingListCoordinator* weakSelf = self;
    if (reauthAgent.authenticationRequired) {
      // Copy C++ args to call later from the block.
      GURL copyEntryURL = GURL(entryURL);
      GURL copyOfflineURL = GURL(offlineURL);
      [reauthAgent
          authenticateIncognitoContentWithCompletionBlock:^(BOOL success) {
            if (success) {
              [weakSelf loadEntryURL:copyEntryURL
                      withOfflineURL:copyOfflineURL
                  loadOfflineVersion:YES
                            inNewTab:newTab
                           incognito:incognito];
            }
          }];
      return;
    }
  }

  DCHECK(entryURL.is_valid());
  base::RecordAction(base::UserMetricsAction("MobileReadingListOpen"));
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  new_tab_page_uma::RecordAction(
      self.browser->GetBrowserState()->IsOffTheRecord(), activeWebState,
      new_tab_page_uma::ACTION_OPENED_READING_LIST_ENTRY);

  // Load the offline URL if available.
  GURL loadURL = entryURL;
  if (offlineURL.is_valid() && !loadOfflineVersion) {
    loadURL = offlineURL;
    // Offline URLs should always be opened in new tabs.
    newTab = YES;
    const GURL updateURL = entryURL;
    [self.mediator markEntryRead:updateURL];
  }

  // Prepare the table for dismissal.
  [self.tableViewController willBeDismissed];

  if (loadOfflineVersion) {
    DCHECK(!newTab);
    OfflinePageTabHelper* offlinePageTabHelper =
        OfflinePageTabHelper::FromWebState(activeWebState);
    if (offlinePageTabHelper &&
        offlinePageTabHelper->CanHandleErrorLoadingURL(entryURL)) {
      offlinePageTabHelper->LoadOfflinePage(entryURL);
    }
    // Use a referrer with a specific URL to signal that this entry should not
    // be taken into account for the Most Visited tiles.
  } else if (newTab) {
    UrlLoadParams params = UrlLoadParams::InNewTab(loadURL, entryURL);
    params.in_incognito = incognito;
    params.web_params.referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                               web::ReferrerPolicyDefault);
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  } else {
    UrlLoadParams params = UrlLoadParams::InCurrentTab(loadURL);
    params.web_params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
    params.web_params.referrer = web::Referrer(GURL(kReadingListReferrerURL),
                                               web::ReferrerPolicyDefault);
    UrlLoadingBrowserAgent::FromBrowser(self.browser)->Load(params);
  }

  [_delegate closeReadingList];
}

- (void)openItemOfflineInNewTab:(id<ReadingListListItem>)item {
  scoped_refptr<const ReadingListEntry> entry =
      [self.mediator entryFromItem:item];
  if (!entry)
    return;

  BOOL offTheRecord = self.browser->GetBrowserState()->IsOffTheRecord();

  if (entry->DistilledState() == ReadingListEntry::PROCESSED) {
    const GURL entryURL = entry->URL();
    GURL offlineURL = reading_list::OfflineURLForURL(entryURL);

    if (web::features::IsLoadSimulatedRequestAPIEnabled()) {
      [self loadEntryURL:entryURL
              withOfflineURL:entryURL
          loadOfflineVersion:YES
                    inNewTab:NO
                   incognito:offTheRecord];
    } else {
      [self loadEntryURL:entryURL
              withOfflineURL:offlineURL
          loadOfflineVersion:NO
                    inNewTab:YES
                   incognito:offTheRecord];
    }
  }
}

#pragma mark - ReadingListMenuProvider

- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (id<ReadingListListItem>)item
                                                      withView:(UIView*)view {
  __weak id<ReadingListListItemAccessibilityDelegate> accessibilityDelegate =
      self.tableViewController;
  __weak __typeof(self) weakSelf = self;

  UIContextMenuActionProvider actionProvider =
      ^(NSArray<UIMenuElement*>* suggestedActions) {
        if (!weakSelf) {
          // Return an empty menu.
          return [UIMenu menuWithTitle:@"" children:@[]];
        }

        ReadingListCoordinator* strongSelf = weakSelf;

        // Record that this context menu was shown to the user.
        RecordMenuShown(MenuScenarioHistogram::kReadingListEntry);

        BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
            initWithBrowser:strongSelf.browser
                   scenario:MenuScenarioHistogram::kReadingListEntry];

        NSMutableArray<UIMenuElement*>* menuElements =
            [[NSMutableArray alloc] init];

        UIAction* openInNewTab = [actionFactory actionToOpenInNewTabWithBlock:^{
          if ([weakSelf isIncognitoForced])
            return;

          [weakSelf loadEntryURL:item.entryURL
                  withOfflineURL:GURL::EmptyGURL()
              loadOfflineVersion:NO
                        inNewTab:YES
                       incognito:NO];
        }];
        if ([self isIncognitoForced]) {
          openInNewTab.attributes = UIMenuElementAttributesDisabled;
        }
        [menuElements addObject:openInNewTab];

        UIAction* openInNewIncognitoTab =
            [actionFactory actionToOpenInNewIncognitoTabWithBlock:^{
              if (![weakSelf isIncognitoAvailable])
                return;

              [weakSelf loadEntryURL:item.entryURL
                      withOfflineURL:GURL::EmptyGURL()
                  loadOfflineVersion:NO
                            inNewTab:YES
                           incognito:YES];
            }];
        if (![self isIncognitoAvailable]) {
          openInNewIncognitoTab.attributes = UIMenuElementAttributesDisabled;
        }
        [menuElements addObject:openInNewIncognitoTab];

        scoped_refptr<const ReadingListEntry> entry =
            [self.mediator entryFromItem:item];
        if (entry && entry->DistilledState() == ReadingListEntry::PROCESSED) {
          [menuElements
              addObject:[actionFactory
                            actionToOpenOfflineVersionInNewTabWithBlock:^{
                              [weakSelf openItemOfflineInNewTab:item];
                            }]];
        }

        if (base::ios::IsMultipleScenesSupported()) {
          [menuElements
              addObject:
                  [actionFactory
                      actionToOpenInNewWindowWithURL:item.entryURL
                                      activityOrigin:
                                          WindowActivityReadingListOrigin]];
        }

        if ([accessibilityDelegate isItemRead:item]) {
          [menuElements
              addObject:[actionFactory actionToMarkAsUnreadWithBlock:^{
                [accessibilityDelegate markItemUnread:item];
              }]];
        } else {
          [menuElements addObject:[actionFactory actionToMarkAsReadWithBlock:^{
                          [accessibilityDelegate markItemRead:item];
                        }]];
        }

        [menuElements addObject:[actionFactory actionToCopyURL:item.entryURL]];

        [menuElements addObject:[actionFactory actionToShareWithBlock:^{
                        [weakSelf shareURL:item.entryURL
                                     title:item.title
                                  fromView:view];
                      }]];

        [menuElements addObject:[actionFactory actionToDeleteWithBlock:^{
                        [accessibilityDelegate deleteItem:item];
                      }]];

        return [UIMenu menuWithTitle:@"" children:menuElements];
      };

  return
      [UIContextMenuConfiguration configurationWithIdentifier:nil
                                              previewProvider:nil
                                               actionProvider:actionProvider];
}

#pragma mark - SigninPresenter

- (void)showSignin:(ShowSigninCommand*)command {
  [_applicationCommandsHandler showSignin:command
                       baseViewController:self.tableViewController];
}

#pragma mark - SigninPromoViewConsumer

- (void)configureSigninPromoWithConfigurator:
            (SigninPromoViewConfigurator*)configurator
                             identityChanged:(BOOL)identityChanged {
  [self.tableViewController
      configureSigninPromoWithConfigurator:configurator
                           identityChanged:identityChanged];
}

- (void)promoProgressStateDidChange {
  [self updateSignInPromoVisibility];
}

- (void)signinDidFinish {
  [self updateSignInPromoVisibility];
}

- (void)signinPromoViewMediatorCloseButtonWasTapped:
    (SigninPromoViewMediator*)mediator {
  [self updateSignInPromoVisibility];
}

// TODO(crbug.com/1425862): This delegate's implementation will be moved to
// SigninPromoViewMediator.
#pragma mark - IdentityManagerObserverBridgeDelegate

// Called when a user changes the syncing state.
- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      if (!_signinPromoViewMediator.showSpinner) {
        self.shouldShowSignInPromo = NO;
      }
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      [self updateSignInPromoVisibility];
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - Private

// Computes whether the sign-in promo should be visible in the reading list and
// updates the view accordingly.
- (void)updateSignInPromoVisibility {
  BOOL areAccountStorageAndPromoEnabled =
      base::FeatureList::IsEnabled(
          syncer::kReadingListEnableDualReadingListModel) &&
      base::FeatureList::IsEnabled(
          syncer::kReadingListEnableSyncTransportModeUponSignIn);
  if (!areAccountStorageAndPromoEnabled || self.isSyncDisabledByAdministrator) {
    self.shouldShowSignInPromo = NO;
    return;
  }

  if (![SigninPromoViewMediator
          shouldDisplaySigninPromoViewWithAccessPoint:
              signin_metrics::AccessPoint::ACCESS_POINT_READING_LIST
                                authenticationService:_authService
                                          prefService:_prefService]) {
    self.shouldShowSignInPromo = NO;
    return;
  }

  if (_identityManager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    // If the user is signed-in with the promo (thus opted-in for Reading List
    // account storage), the promo should stay visible during the initial sync
    // and a spinner should be shown on it.
    self.shouldShowSignInPromo = _signinPromoViewMediator.showSpinner;
  } else {
    const std::string lastSignedInGaiaId =
        _prefService->GetString(prefs::kGoogleServicesLastSyncingGaiaId);
    // If the last signed-in user did not remove data during sign-out, don't
    // show the signin promo.
    self.shouldShowSignInPromo = lastSignedInGaiaId.empty();
  }
}

// Updates the visibility of the sign-in promo.
- (void)setShouldShowSignInPromo:(BOOL)shouldShowSignInPromo {
  if (_shouldShowSignInPromo == shouldShowSignInPromo) {
    return;
  }

  _shouldShowSignInPromo = shouldShowSignInPromo;
  SigninPromoViewConfigurator* promoConfigurator =
      [_signinPromoViewMediator createConfigurator];
  [self.tableViewController promoStateChanged:shouldShowSignInPromo
                            promoConfigurator:promoConfigurator
                                promoDelegate:_signinPromoViewMediator];
  if (shouldShowSignInPromo) {
    [_signinPromoViewMediator signinPromoViewIsVisible];
  } else {
    if (!_signinPromoViewMediator.invalidClosedOrNeverVisible) {
      [_signinPromoViewMediator signinPromoViewIsHidden];
    }
  }
}

// Triggers the URL sharing flow for the given `URL` and `title`, with the
// origin `view` representing the UI component for that URL.
- (void)shareURL:(const GURL&)URL
           title:(NSString*)title
        fromView:(UIView*)view {
  SharingParams* params =
      [[SharingParams alloc] initWithURL:URL
                                   title:title
                                scenario:SharingScenario::ReadingListEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.tableViewController
                         browser:self.browser
                          params:params
                      originView:view];
  [self.sharingCoordinator start];
}

// Returns YES if the user cannot turn on sync for enterprise policy reasons.
- (BOOL)isSyncDisabledByAdministrator {
  const bool syncDisabledPolicy = _syncService->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
  const bool syncTypesDisabledPolicy =
      _syncService->GetUserSettings()->IsTypeManagedByPolicy(
          syncer::UserSelectableType::kReadingList);
  return syncDisabledPolicy || syncTypesDisabledPolicy;
}

#pragma mark - ReadingListListItemFactoryDelegate

- (BOOL)isIncognitoForced {
  return IsIncognitoModeForced(_prefService);
}

- (BOOL)isIncognitoAvailable {
  return !IsIncognitoModeDisabled(_prefService);
}

@end
