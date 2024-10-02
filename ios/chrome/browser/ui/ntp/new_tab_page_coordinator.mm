// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/feature_list.h"
#import "base/metrics/field_trial_params.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/time/time.h"
#import "components/feed/core/v2/public/common_enums.h"
#import "components/feed/core/v2/public/ios/pref_names.h"
#import "components/policy/policy_constants.h"
#import "components/pref_registry/pref_registry_syncable.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/search_engines/default_search_manager.h"
#import "components/search_engines/template_url.h"
#import "components/search_engines/template_url_service.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/signin/public/identity_manager/objc/identity_manager_observer_bridge.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/discover_feed/discover_feed_observer_bridge.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service.h"
#import "ios/chrome/browser/discover_feed/discover_feed_service_factory.h"
#import "ios/chrome/browser/discover_feed/feed_constants.h"
#import "ios/chrome/browser/discover_feed/feed_model_configuration.h"
#import "ios/chrome/browser/follow/follow_browser_agent.h"
#import "ios/chrome/browser/follow/followed_web_site.h"
#import "ios/chrome/browser/follow/followed_web_site_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/show_signin_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/named_guide.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/signin/capabilities_types.h"
#import "ios/chrome/browser/signin/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/signin/system_identity_manager.h"
#import "ios/chrome/browser/sync/sync_service_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_coordinator.h"
#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_feature.h"
#import "ios/chrome/browser/ui/context_menu/link_preview/link_preview_coordinator.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_constants.h"
#import "ios/chrome/browser/ui/ntp/discover_feed_preview_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_control_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_management/feed_management_navigation_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_menu_commands.h"
#import "ios/chrome/browser/ui/ntp/feed_promos/feed_sign_in_promo_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_sign_in_promo_delegate.h"
#import "ios/chrome/browser/ui/ntp/feed_top_section/feed_top_section_coordinator.h"
#import "ios/chrome/browser/ui/ntp/feed_wrapper_view_controller.h"
#import "ios/chrome/browser/ui/ntp/incognito/incognito_view_controller.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_constants.h"
#import "ios/chrome/browser/ui/ntp/metrics/feed_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/metrics/new_tab_page_metrics_recorder.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_component_factory_protocol.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_content_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_coordinator+private.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_feature.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_follow_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_commands.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_header_view_controller.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_mediator.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_metrics_delegate.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_view_controller.h"
#import "ios/chrome/browser/ui/overscroll_actions/overscroll_actions_controller.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/toolbar/public/fakebox_focuser.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/ui_utils/ui_utils_api.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state_observer_bridge.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
bool IsNTPActiveForWebState(web::WebState* web_state) {
  if (!web_state) {
    return false;
  }
  NewTabPageTabHelper* helper = NewTabPageTabHelper::FromWebState(web_state);
  return helper && helper->IsActive();
}
}  // namespace

@interface NewTabPageCoordinator () <AppStateObserver,
                                     BooleanObserver,
                                     DiscoverFeedObserverBridgeDelegate,
                                     DiscoverFeedPreviewDelegate,
                                     FeedControlDelegate,
                                     FeedDelegate,
                                     FeedManagementNavigationDelegate,
                                     FeedMenuCommands,
                                     FeedSignInPromoDelegate,
                                     FeedWrapperViewControllerDelegate,
                                     IdentityManagerObserverBridgeDelegate,
                                     NewTabPageContentDelegate,
                                     NewTabPageDelegate,
                                     NewTabPageFollowDelegate,
                                     NewTabPageHeaderCommands,
                                     NewTabPageMetricsDelegate,
                                     OverscrollActionsControllerDelegate,
                                     PrefObserverDelegate,
                                     SceneStateObserver> {
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  std::unique_ptr<PrefChangeRegistrar> _prefChangeRegistrar;

  // Observes changes in the IdentityManager.
  std::unique_ptr<signin::IdentityManagerObserverBridge>
      _identityObserverBridge;

  // Observes changes in the DiscoverFeed.
  std::unique_ptr<DiscoverFeedObserverBridge> _discoverFeedObserverBridge;

  // Bridges C++ WebStateListObserver methods to this NewTabPageCoordinator.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserver;
}

// Coordinator for the ContentSuggestions.
@property(nonatomic, strong)
    ContentSuggestionsCoordinator* contentSuggestionsCoordinator;

// View controller for the regular NTP.
@property(nonatomic, strong) NewTabPageViewController* NTPViewController;

// Mediator owned by this coordinator.
@property(nonatomic, strong) NewTabPageMediator* NTPMediator;

// View controller wrapping the feed.
@property(nonatomic, strong)
    FeedWrapperViewController* feedWrapperViewController;

// View controller for the incognito NTP.
@property(nonatomic, strong) IncognitoViewController* incognitoViewController;

// The timetick of the last time the NTP was displayed.
@property(nonatomic, assign) base::TimeTicks didAppearTime;

// Tracks the visibility of the NTP to report NTP usage metrics.
// True if the NTP view is currently displayed to the user.
// Redefined to readwrite.
@property(nonatomic, assign, readwrite) BOOL visible;

// Whether the view is new tab view is currently presented (possibly in
// background). Used to report NTP usage metrics.
@property(nonatomic, assign) BOOL viewPresented;

// Wheter the scene is currently in foreground.
@property(nonatomic, assign) BOOL sceneInForeground;

// The ViewController displayed by this Coordinator. This is the returned
// ViewController and will contain the `containedViewController` (Which can
// change depending on Feed visibility).
@property(nonatomic, strong) UIViewController* containerViewController;

// The coordinator contained ViewController.
@property(nonatomic, strong) UIViewController* containedViewController;

// PrefService used by this Coordinator.
@property(nonatomic, assign) PrefService* prefService;

// Whether the feed is expanded or collapsed. Collapsed
// means the feed header is shown, but not any of the feed content.
@property(nonatomic, strong) PrefBackedBoolean* feedExpandedPref;

// The view controller representing the selected feed, such as the Discover or
// Following feed.
@property(nonatomic, weak) UIViewController* feedViewController;

// The Coordinator to display previews for Discover feed websites. It also
// handles the actions related to them.
@property(nonatomic, strong) LinkPreviewCoordinator* linkPreviewCoordinator;

// The Coordinator to display Sign In promo UI.
@property(nonatomic, strong)
    FeedSignInPromoCoordinator* feedSignInPromoCoordinator;

// The view controller representing the NTP feed header.
@property(nonatomic, strong) FeedHeaderViewController* feedHeaderViewController;

// Alert coordinator for handling the feed header menu.
@property(nonatomic, strong) ActionSheetCoordinator* alertCoordinator;

// Authentication Service for the user's signed-in state.
@property(nonatomic, assign) AuthenticationService* authService;

// TemplateURL used to get the search engine.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// DiscoverFeed Service to display the Feed.
@property(nonatomic, assign) DiscoverFeedService* discoverFeedService;

// Metrics recorder for actions relating to the feed.
@property(nonatomic, strong) FeedMetricsRecorder* feedMetricsRecorder;

// The header view controller containing the fake omnibox and logo.
@property(nonatomic, strong)
    NewTabPageHeaderViewController* headerViewController;

// The coordinator for handling feed management.
@property(nonatomic, strong)
    FeedManagementCoordinator* feedManagementCoordinator;

// Coordinator for Feed top section.
@property(nonatomic, strong)
    FeedTopSectionCoordinator* feedTopSectionCoordinator;

// Currently selected feed. Redefined to readwrite.
@property(nonatomic, assign, readwrite) FeedType selectedFeed;

// The Webstate associated with this coordinator.
@property(nonatomic, assign) web::WebState* webState;

// Returns `YES` if the coordinator is started.
@property(nonatomic, assign) BOOL started;

// Contains a factory which can generate NTP components which are initialized
// on `start`.
@property(nonatomic, strong) id<NewTabPageComponentFactoryProtocol>
    componentFactory;

// Recorder for new tab page metrics.
@property(nonatomic, strong) NewTabPageMetricsRecorder* NTPMetricsRecorder;

@end

@implementation NewTabPageCoordinator

// Synthesize NewTabPageConfiguring properties.
@synthesize shouldScrollIntoFeed = _shouldScrollIntoFeed;
@synthesize baseViewController = _baseViewController;

#pragma mark - ChromeCoordinator

- (instancetype)initWithBrowser:(Browser*)browser
               componentFactory:
                   (id<NewTabPageComponentFactoryProtocol>)componentFactory {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    _componentFactory = componentFactory;
    _containerViewController = [[UIViewController alloc] init];
  }
  return self;
}

- (void)start {
  if (self.started) {
    return;
  }

  DCHECK(self.browser);
  DCHECK(self.toolbarDelegate);
  DCHECK(!self.contentSuggestionsCoordinator);

  self.webState = self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(self.webState);
  DCHECK(NewTabPageTabHelper::FromWebState(self.webState)->IsActive());

  // Start observing WebStateList changes.
  _webStateListObserver = std::make_unique<WebStateListObserverBridge>(self);
  self.browser->GetWebStateList()->AddObserver(_webStateListObserver.get());

  // Start observing SceneState changes.
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState addObserver:self];
  self.sceneInForeground =
      sceneState.activationLevel >= SceneActivationLevelForegroundInactive;
  // Configures incognito NTP if user is in incognito mode.
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    DCHECK(!self.incognitoViewController);
    UrlLoadingBrowserAgent* URLLoader =
        UrlLoadingBrowserAgent::FromBrowser(self.browser);
    self.incognitoViewController =
        [[IncognitoViewController alloc] initWithUrlLoader:URLLoader];
    self.started = YES;
    [self NTPDidChangeVisibility:YES];
    return;
  }

  self.selectedFeed =
      NewTabPageTabHelper::FromWebState(self.webState)->GetNextNTPFeedType();

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  [self initializeServices];
  [self initializeNTPComponents];
  [self startObservers];

  // Do not focus on omnibox for voice over if there are other screens to
  // show.
  AppState* appState = sceneState.appState;
  [appState addObserver:self];
  if (appState.initStage < InitStageFinal) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = NO;
  }

  // Updates feed asynchronously if the account is subject to parental controls.
  [self updateFeedVisibilityForSupervision];

  // Configures NTP components.
  if ([self isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  }
  [self configureHeaderViewController];
  [self configureContentSuggestionsCoordinator];
  [self configureNTPMediator];
  [self configureFeedMetricsRecorder];
  [self configureNTPViewController];

  self.started = YES;
  [self NTPDidChangeVisibility:YES];
}

- (void)stop {
  if (!self.started)
    return;

  self.browser->GetWebStateList()->RemoveObserver(_webStateListObserver.get());
  _webStateListObserver.reset();
  _webState = nullptr;

  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  [sceneState removeObserver:self];

  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    self.incognitoViewController = nil;
    self.started = NO;
    self.viewPresented = NO;
    [self updateVisible];
    return;
  }

  // NOTE: anything that executes below WILL NOT execute for OffTheRecord
  // browsers!

  [sceneState.appState removeObserver:self];

  self.viewPresented = NO;
  [self updateVisible];

  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;
  [self.contentSuggestionsCoordinator stop];
  self.contentSuggestionsCoordinator = nil;
  self.headerViewController = nil;
  // Remove before nil to ensure View Hierarchy doesn't hold last strong
  // reference.
  [self.containedViewController willMoveToParentViewController:nil];
  [self.containedViewController.view removeFromSuperview];
  [self.containedViewController removeFromParentViewController];
  self.containedViewController = nil;
  self.NTPViewController.feedHeaderViewController = nil;
  self.NTPViewController = nil;
  self.feedHeaderViewController.ntpDelegate = nil;
  self.feedHeaderViewController = nil;
  [self.feedTopSectionCoordinator stop];
  self.feedTopSectionCoordinator = nil;

  self.NTPMetricsRecorder = nil;

  if (self.feedSignInPromoCoordinator) {
    [self.feedSignInPromoCoordinator stop];
    self.feedSignInPromoCoordinator = nil;
  }

  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;

  self.alertCoordinator = nil;
  self.authService = nil;
  self.templateURLService = nil;

  [self.NTPMediator shutdown];
  self.NTPMediator = nil;

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedMetricsRecorder = nil;

  [self.feedExpandedPref setObserver:nil];
  self.feedExpandedPref = nil;

  _prefChangeRegistrar.reset();
  _prefObserverBridge.reset();
  _discoverFeedObserverBridge.reset();
  _identityObserverBridge.reset();

  self.started = NO;
}

#pragma mark - Public

- (void)stopIfNeeded {
  WebStateList* webStateList = self.browser->GetWebStateList();
  for (int i = 0; i < webStateList->count(); i++) {
    NewTabPageTabHelper* iterNtpHelper =
        NewTabPageTabHelper::FromWebState(webStateList->GetWebStateAt(i));
    if (iterNtpHelper->IsActive()) {
      return;
    }
  }

  // No active NTPs were found.
  [self stop];
}

- (void)stopScrolling {
  if (!self.contentSuggestionsCoordinator) {
    return;
  }
  [self.NTPViewController stopScrolling];
}

- (BOOL)isScrolledToTop {
  return [self.NTPViewController isNTPScrolledToTop];
}

- (void)willUpdateSnapshot {
  if (self.contentSuggestionsCoordinator.started) {
    [self.NTPViewController willUpdateSnapshot];
  }
}

- (void)focusFakebox {
  [self.NTPViewController focusOmnibox];
}

- (void)reload {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  // Call this before RefreshFeed() to ensure some NTP state configs are reset
  // before callbacks in repsonse to a feed refresh are called, ensuring the NTP
  // returns to a state at the top of the surface upon refresh.
  [self.NTPViewController resetStateUponReload];
  self.discoverFeedService->RefreshFeed(
      FeedRefreshTrigger::kForegroundUserTriggered);
  [self reloadContentSuggestions];
}

- (void)locationBarDidBecomeFirstResponder {
  [self.headerViewController locationBarBecomesFirstResponder];
  self.NTPViewController.omniboxFocused = YES;
}

- (void)locationBarDidResignFirstResponder {
  // Do not trigger defocus animation if the user is already navigating away
  // from the NTP.
  if (self.viewPresented) {
    [self.NTPViewController omniboxDidResignFirstResponder];
  }
}

- (void)constrainDiscoverHeaderMenuButtonNamedGuide {
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return;
  }
  [LayoutGuideCenterForBrowser(self.browser)
      referenceView:self.feedHeaderViewController.menuButton
          underName:kDiscoverFeedHeaderMenuGuide];
}

- (void)updateFollowingFeedHasUnseenContent:(BOOL)hasUnseenContent {
  if (![self isFollowingFeedAvailable] ||
      !IsDotEnabledForNewFollowedContent()) {
    return;
  }
  if ([self doesFollowingFeedHaveContent]) {
    [self.feedHeaderViewController
        updateFollowingSegmentDotForUnseenContent:hasUnseenContent];
  }
}

- (void)handleFeedModelDidEndUpdates:(FeedType)feedType {
  DCHECK(self.NTPViewController);
  if (!self.feedViewController) {
    return;
  }
  // When the visible feed has been updated, recalculate the minimum NTP height.
  if (feedType == self.selectedFeed) {
    [self.NTPViewController feedLayoutDidEndUpdates];
  }
}

- (void)didNavigateToNTP {
  if (self.started) {
    self.webState = self.browser->GetWebStateList()->GetActiveWebState();
    [self NTPDidChangeVisibility:YES];
  }
}

- (void)didNavigateAwayFromNTP {
  [self NTPDidChangeVisibility:NO];
  self.webState = nullptr;
  [self stopIfNeeded];
}

#pragma mark - Setters

- (void)setSelectedFeed:(FeedType)selectedFeed {
  if (_selectedFeed == selectedFeed) {
    return;
  }
  // Tell Metrics Recorder the feed has changed.
  [self.feedMetricsRecorder recordFeedTypeChangedFromFeed:_selectedFeed];
  _selectedFeed = selectedFeed;
}

#pragma mark - Initializers

// Gets all NTP services from the browser state.
- (void)initializeServices {
  self.authService = AuthenticationServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.templateURLService = ios::TemplateURLServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.discoverFeedService = DiscoverFeedServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.prefService =
      ChromeBrowserState::FromBrowserState(self.browser->GetBrowserState())
          ->GetPrefs();
}

// Starts all NTP observers.
- (void)startObservers {
  DCHECK(self.prefService);
  DCHECK(self.headerViewController);

  _prefChangeRegistrar = std::make_unique<PrefChangeRegistrar>();
  _prefChangeRegistrar->Init(self.prefService);
  _prefObserverBridge = std::make_unique<PrefObserverBridge>(self);
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kArticlesForYouEnabled, _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kNTPContentSuggestionsEnabled, _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
      _prefChangeRegistrar.get());
  _prefObserverBridge->ObserveChangesForPreference(
      DefaultSearchManager::kDefaultSearchProviderDataPrefName,
      _prefChangeRegistrar.get());
  self.feedExpandedPref = [[PrefBackedBoolean alloc]
      initWithPrefService:self.prefService
                 prefName:feed::prefs::kArticlesListVisible];
  // Observer is necessary for multiwindow NTPs to remain in sync.
  [self.feedExpandedPref setObserver:self];

  // Start observing IdentityManager.
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  _identityObserverBridge =
      std::make_unique<signin::IdentityManagerObserverBridge>(identityManager,
                                                              self);

  // Start observing DiscoverFeedService.
  _discoverFeedObserverBridge = std::make_unique<DiscoverFeedObserverBridge>(
      self, self.discoverFeedService);
}

// Creates all the NTP components.
- (void)initializeNTPComponents {
  Browser* browser = self.browser;
  id<NewTabPageComponentFactoryProtocol> componentFactory =
      self.componentFactory;
  self.NTPViewController = [componentFactory NTPViewController];
  self.headerViewController = [componentFactory headerViewController];
  self.NTPMediator =
      [componentFactory NTPMediatorForBrowser:browser
                                     webState:self.webState
                     identityDiscImageUpdater:self.headerViewController];
  self.contentSuggestionsCoordinator =
      [componentFactory contentSuggestionsCoordinatorForBrowser:browser];
  self.feedMetricsRecorder =
      [componentFactory feedMetricsRecorderForBrowser:browser];
  self.NTPMetricsRecorder = [[NewTabPageMetricsRecorder alloc] init];
}

#pragma mark - Configurators

// Creates and configures the feed and feed header based on user prefs.
- (void)configureFeedAndHeader {
  DCHECK([self isFeedHeaderVisible]);
  DCHECK(self.NTPViewController);

  self.NTPViewController.feedHeaderViewController =
      self.feedHeaderViewController;

  // Requests feeds here if the correct flags and prefs are enabled.
  if ([self shouldFeedBeVisible]) {
    if ([self isFollowingFeedAvailable]) {
      switch (self.selectedFeed) {
        case FeedTypeDiscover:
          self.feedViewController = [self.componentFactory
                   discoverFeedForBrowser:self.browser
              viewControllerConfiguration:[self
                                              feedViewControllerConfiguration]];
          break;
        case FeedTypeFollowing:
          self.feedViewController = [self.componentFactory
                  followingFeedForBrowser:self.browser
              viewControllerConfiguration:[self feedViewControllerConfiguration]
                                 sortType:self.followingFeedSortType];
          break;
      }
    } else {
      self.feedViewController = [self.componentFactory
               discoverFeedForBrowser:self.browser
          viewControllerConfiguration:[self feedViewControllerConfiguration]];
    }
  }

  // Feed top section visibility is based on feed visibility, so this should
  // always be below the block that sets `feedViewController`.
  if ([self isFeedTopSectionVisible]) {
    self.feedTopSectionCoordinator = [self createFeedTopSectionCoordinator];
  }
}

// Configures `self.headerViewController`.
- (void)configureHeaderViewController {
  DCHECK(self.headerViewController);
  DCHECK(self.NTPMediator);
  DCHECK(self.NTPMetricsRecorder);

  self.headerViewController.isGoogleDefaultSearchEngine =
      [self isGoogleDefaultSearchEngine];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.headerViewController.dispatcher =
      static_cast<id<ApplicationCommands, BrowserCoordinatorCommands,
                     OmniboxCommands, FakeboxFocuser, LensCommands>>(
          self.browser->GetCommandDispatcher());
  self.headerViewController.commandHandler = self;
  self.headerViewController.delegate = self.NTPViewController;
  self.headerViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.headerViewController.toolbarDelegate = self.toolbarDelegate;
  self.headerViewController.baseViewController = self.baseViewController;
  self.headerViewController.NTPMetricsRecorder = self.NTPMetricsRecorder;
}

// Configures `self.contentSuggestionsCoordiantor`.
- (void)configureContentSuggestionsCoordinator {
  self.contentSuggestionsCoordinator.webState = self.webState;
  self.contentSuggestionsCoordinator.NTPDelegate = self;
  self.contentSuggestionsCoordinator.feedDelegate = self;
  self.contentSuggestionsCoordinator.NTPMetricsDelegate = self;
  [self.contentSuggestionsCoordinator start];
}

// Configures `self.NTPMediator`.
- (void)configureNTPMediator {
  NewTabPageMediator* NTPMediator = self.NTPMediator;
  DCHECK(NTPMediator);
  NTPMediator.browser = self.browser;
  NTPMediator.feedControlDelegate = self;
  NTPMediator.headerConsumer = self.headerViewController;
  NTPMediator.consumer = self.NTPViewController;
  NTPMediator.suggestionsMediator =
      self.contentSuggestionsCoordinator.contentSuggestionsMediator;
  [NTPMediator setUp];
}

// Configures `self.feedMetricsRecorder`.
- (void)configureFeedMetricsRecorder {
  self.feedMetricsRecorder.feedControlDelegate = self;
  self.feedMetricsRecorder.followDelegate = self;
  self.feedMetricsRecorder.NTPMetricsDelegate = self;
}

// Configures `self.NTPViewController` and sets it up as the main ViewController
// managed by this Coordinator.
- (void)configureNTPViewController {
  DCHECK(self.NTPViewController);

  self.NTPViewController.contentSuggestionsViewController =
      self.contentSuggestionsCoordinator.viewController;

  self.NTPViewController.panGestureHandler = self.panGestureHandler;
  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [self.componentFactory
      feedWrapperViewControllerWithDelegate:self
                         feedViewController:self.feedViewController];

  if ([self isFeedTopSectionVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;
  self.NTPViewController.overscrollDelegate = self;
  self.NTPViewController.ntpContentDelegate = self;

  self.NTPViewController.headerViewController = self.headerViewController;

  [self configureMainViewControllerUsing:self.NTPViewController];
  self.NTPViewController.feedMetricsRecorder = self.feedMetricsRecorder;
  self.NTPViewController.bubblePresenter = self.bubblePresenter;
}

// Configures the main ViewController managed by this Coordinator.
- (void)configureMainViewControllerUsing:
    (UIViewController*)containedViewController {
  [containedViewController
      willMoveToParentViewController:self.containerViewController];
  [self.containerViewController addChildViewController:containedViewController];
  [self.containerViewController.view addSubview:containedViewController.view];
  [containedViewController
      didMoveToParentViewController:self.containerViewController];

  containedViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(containedViewController.view,
                     self.containerViewController.view);

  self.containedViewController = containedViewController;
}

#pragma mark - Properties

- (UIViewController*)viewController {
  DCHECK(self.started);
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    return self.incognitoViewController;
  } else {
    return self.containerViewController;
  }
}

- (id<ThumbStripSupporting>)thumbStripSupporting {
  return self.NTPViewController;
}

#pragma mark - NewTabPageConfiguring

- (void)selectFeedType:(FeedType)feedType {
  if (!self.NTPViewController.viewDidAppear ||
      ![self isFollowingFeedAvailable]) {
    self.selectedFeed = feedType;
    return;
  }
  [self handleFeedSelected:feedType];
}

#pragma mark - NewTabPageHeaderCommands

- (void)updateForHeaderSizeChange {
  [self.NTPViewController updateHeightAboveFeedAndScrollToTopIfNeeded];
}

- (void)fakeboxTapped {
  [self.NTPMetricsRecorder recordHomeActionType:IOSHomeActionType::kFakebox
                                 onStartSurface:[self isStartSurface]];
  [self focusFakebox];
}

- (void)identityDiscWasTapped {
  [self.NTPMetricsRecorder recordIdentityDiscTapped];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  BOOL isSignedIn =
      self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
  BOOL isSigninNotAllowed = self.authService->GetServiceStatus() !=
                            AuthenticationService::ServiceStatus::SigninAllowed;
  BOOL isSyncDisabled = IsSyncDisabledByPolicy(
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState()));
  if (isSignedIn || isSigninNotAllowed || isSyncDisabled) {
    [handler showSettingsFromViewController:self.baseViewController];
  } else {
    ShowSigninCommand* const showSigninCommand = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperationSigninAndSync
              accessPoint:signin_metrics::AccessPoint::
                              ACCESS_POINT_NTP_SIGNED_OUT_ICON];
    [handler showSignin:showSigninCommand
        baseViewController:self.baseViewController];
  }
}

#pragma mark - FeedMenuCommands

- (void)openFeedMenu {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;

  // This button is in a container view that itself is in FeedHeaderVC's main
  // view. In order to anchor the alert view correctly, we need to provide the
  // button's frame as well as its superview which is the coordinate system
  // for the frame.
  UIButton* menuButton = self.feedHeaderViewController.menuButton;
  self.alertCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser
                           title:nil
                         message:nil
                            rect:menuButton.frame
                            view:menuButton.superview];
  __weak NewTabPageCoordinator* weakSelf = self;

  // Item for toggling the feed on/off.
  if ([self.feedExpandedPref value]) {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_DISCOVER_FEED_MENU_TURN_OFF_ITEM)
                  action:^{
                    [weakSelf setFeedVisibleFromHeader:NO];
                  }
                   style:UIAlertActionStyleDestructive];
  } else {
    [self.alertCoordinator
        addItemWithTitle:l10n_util::GetNSString(
                             IDS_IOS_DISCOVER_FEED_MENU_TURN_ON_ITEM)
                  action:^{
                    [weakSelf setFeedVisibleFromHeader:YES];
                  }
                   style:UIAlertActionStyleDefault];
  }

  // Items for signed-in users.
  if (self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin)) {
    if ([self isFollowingFeedAvailable]) {
      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ITEM)
                    action:^{
                      [weakSelf handleFeedManageTapped];
                    }
                     style:UIAlertActionStyleDefault];
    } else {
      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_ACTIVITY_ITEM)
                    action:^{
                      [weakSelf.NTPMediator handleFeedManageActivityTapped];
                    }
                     style:UIAlertActionStyleDefault];

      [self.alertCoordinator
          addItemWithTitle:l10n_util::GetNSString(
                               IDS_IOS_DISCOVER_FEED_MENU_MANAGE_INTERESTS_ITEM)
                    action:^{
                      [weakSelf.NTPMediator handleFeedManageInterestsTapped];
                    }
                     style:UIAlertActionStyleDefault];
    }
  }

  // Items for all users.
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(
                           IDS_IOS_DISCOVER_FEED_MENU_LEARN_MORE_ITEM)
                action:^{
                  [weakSelf.NTPMediator handleFeedLearnMoreTapped];
                }
                 style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

#pragma mark - DiscoverFeedPreviewDelegate

- (UIViewController*)discoverFeedPreviewWithURL:(const GURL)URL {
  std::string referrerURL = base::GetFieldTrialParamValueByFeature(
      kOverrideFeedSettings, kFeedSettingDiscoverReferrerParameter);
  if (referrerURL.empty()) {
    referrerURL = kDefaultDiscoverReferrer;
  }

  self.linkPreviewCoordinator =
      [[LinkPreviewCoordinator alloc] initWithBrowser:self.browser URL:URL];
  self.linkPreviewCoordinator.referrer =
      web::Referrer(GURL(referrerURL), web::ReferrerPolicyDefault);
  [self.linkPreviewCoordinator start];
  return [self.linkPreviewCoordinator linkPreviewViewController];
}

- (void)didTapDiscoverFeedPreview {
  DCHECK(self.linkPreviewCoordinator);
  [self.linkPreviewCoordinator handlePreviewAction];
  [self.linkPreviewCoordinator stop];
  self.linkPreviewCoordinator = nil;
}

#pragma mark - FeedControlDelegate

- (FollowingFeedSortType)followingFeedSortType {
  if (IsFollowingFeedDefaultSortTypeEnabled()) {
    BOOL hasDefaultBeenChanged = self.prefService->GetBoolean(
        prefs::kDefaultFollowingFeedSortTypeChanged);
    if (hasDefaultBeenChanged) {
      return (FollowingFeedSortType)self.prefService->GetInteger(
          prefs::kNTPFollowingFeedSortType);
    } else {
      return IsDefaultFollowingFeedSortTypeGroupedByPublisher()
                 ? FollowingFeedSortTypeByPublisher
                 : FollowingFeedSortTypeByLatest;
    }
  }
  // TODO(crbug.com/1352935): Add a DCHECK to make sure the coordinator isn't
  // stopped when we check this. That would require us to use the NTPHelper to
  // get this information.
  return (FollowingFeedSortType)self.prefService->GetInteger(
      prefs::kNTPFollowingFeedSortType);
}

- (void)handleFeedSelected:(FeedType)feedType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.selectedFeed == feedType) {
    return;
  }
  self.selectedFeed = feedType;

  // Saves scroll position before changing feed.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  if (feedType == FeedTypeFollowing && IsDotEnabledForNewFollowedContent()) {
    // Clears dot and notifies service that the Following feed content has
    // been seen.
    [self.feedHeaderViewController
        updateFollowingSegmentDotForUnseenContent:NO];
    self.discoverFeedService->SetFollowingFeedContentSeen();
  }

  [self updateNTPForFeed];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeed:scrollPosition];
}

- (void)handleSortTypeForFollowingFeed:(FollowingFeedSortType)sortType {
  DCHECK([self isFollowingFeedAvailable]);

  if (self.feedHeaderViewController.followingFeedSortType == sortType) {
    return;
  }

  // Save the scroll position before changing sort type.
  CGFloat scrollPosition = [self.NTPViewController scrollPosition];

  [self.feedMetricsRecorder recordFollowingFeedSortTypeSelected:sortType];
  self.prefService->SetInteger(prefs::kNTPFollowingFeedSortType, sortType);
  self.prefService->SetBoolean(prefs::kDefaultFollowingFeedSortTypeChanged,
                               true);
  self.discoverFeedService->SetFollowingFeedSortType(sortType);
  self.feedHeaderViewController.followingFeedSortType = sortType;

  [self updateNTPForFeed];

  // Scroll position resets when changing the feed, so we set it back to what it
  // was.
  [self.NTPViewController setContentOffsetToTopOfFeed:scrollPosition];
}

- (BOOL)shouldFeedBeVisible {
  return [self isFeedHeaderVisible] && [self.feedExpandedPref value];
}

- (BOOL)isFollowingFeedAvailable {
  return IsWebChannelsEnabled() && self.authService &&
         self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

- (NSUInteger)lastVisibleFeedCardIndex {
  return [self.feedWrapperViewController lastVisibleFeedCardIndex];
}

#pragma mark - FeedDelegate

- (void)contentSuggestionsWasUpdated {
  [self.NTPViewController updateHeightAboveFeedAndScrollToTopIfNeeded];
}

#pragma mark - FeedManagementNavigationDelegate

- (void)handleNavigateToActivity {
  [self.NTPMediator handleFeedManageActivityTapped];
}

- (void)handleNavigateToInterests {
  [self.NTPMediator handleFeedManageInterestsTapped];
}

- (void)handleNavigateToHidden {
  [self.NTPMediator handleFeedManageHiddenTapped];
}

- (void)handleNavigateToFollowedURL:(const GURL&)url {
  [self.NTPMediator handleVisitSiteFromFollowManagementList:url];
}

#pragma mark - FeedSignInPromoDelegate

- (void)showSignInPromoUI {
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());

  BOOL hasUserIdentities = accountManagerService->HasIdentities();

  if ([self isSignInAllowed] &&
      (IsConsistencyNewAccountInterfaceEnabled() || hasUserIdentities)) {
    // Show Sign-In only flow, since Sync is not needed for this feature.
    // TODO(crbug.com/1382615): Currently we show sign-in only UI when it's
    // enabled, or when the user has one or more device-level user identities,
    // and when sign-in is allowed. Remove the user identity check when sign-in
    // only flow is fully launched.
    const signin_metrics::AccessPoint access_point =
        signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO;
    id<ApplicationCommands> handler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    ShowSigninCommand* command = [[ShowSigninCommand alloc]
        initWithOperation:AuthenticationOperationSigninOnly
              accessPoint:access_point];
    signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
    [handler showSignin:command baseViewController:self.NTPViewController];
    [self.feedMetricsRecorder
        recordShowSignInOnlyUIWithUserId:hasUserIdentities];
    [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                  feed::FeedSignInUI::kShowSignInOnlyFlow];
  } else if ([self isSignInAllowed] && [self isSyncAllowed]) {
    // Show a sign-in promo half sheet for feed BoC sign-in promo when the
    // condition of showing sign-in only flow is not fulfilled. This UI will
    // lead to sync flow,
    // TODO(crbug.com/1382615): remove this else if block and
    // FeedSignInPromoCoordinator class when sign-in only flow is fully
    // launched.
    self.feedSignInPromoCoordinator = [[FeedSignInPromoCoordinator alloc]
        initWithBaseViewController:self.NTPViewController
                           browser:self.browser];
    [self.feedSignInPromoCoordinator start];
    [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                  feed::FeedSignInUI::kShowSyncHalfSheet];
  } else {
    // Show a snackbar message if sign-in or sync is disabled and the above UI
    // shouldn't be shown.
    [self showSignInDisableMessage];
    [self.feedMetricsRecorder recordShowSignInRelatedUIWithType:
                                  feed::FeedSignInUI::kShowSignInDisableToast];
  }
}

- (void)showSignInUI {
  // Show a snackbar message if sign-in or sync is disabled.
  if (![self isSignInAllowed] || ![self isSyncAllowed]) {
    [self showSignInDisableMessage];
    [self.feedMetricsRecorder recordShowSyncnRelatedUIWithType:
                                  feed::FeedSyncPromo::kShowDisableToast];
    return;
  }

  // Show sync flow.
  const signin_metrics::AccessPoint access_point =
      signin_metrics::AccessPoint::ACCESS_POINT_NTP_FEED_BOTTOM_PROMO;
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  ShowSigninCommand* command = [[ShowSigninCommand alloc]
      initWithOperation:AuthenticationOperationSigninAndSync
            accessPoint:access_point];
  signin_metrics::RecordSigninUserActionForAccessPoint(access_point);
  [handler showSignin:command baseViewController:self.NTPViewController];
  [self.feedMetricsRecorder
      recordShowSyncnRelatedUIWithType:feed::FeedSyncPromo::kShowSyncFlow];
}

#pragma mark - FeedWrapperViewControllerDelegate

- (void)updateTheme {
  self.discoverFeedService->UpdateTheme();
}

#pragma mark - NewTabPageContentDelegate

- (void)reloadContentSuggestions {
  // No need to reload ContentSuggestions since the mediator receives all
  // model state changes and immediately updates the consumer with the new
  // state.
  return;
}

- (BOOL)isContentHeaderSticky {
  return [self isFollowingFeedAvailable] && [self isFeedHeaderVisible] &&
         !IsStickyHeaderDisabledForFollowingFeed();
}

- (void)signinPromoHasChangedVisibility:(BOOL)visible {
  [self.feedTopSectionCoordinator signinPromoHasChangedVisibility:visible];
}

- (void)cancelOmniboxEdit {
  id<OmniboxCommands> omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandHandler cancelOmniboxEdit];
}

- (void)onFakeboxBlur {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler onFakeboxBlur];
}

- (void)focusOmnibox {
  id<FakeboxFocuser> fakeboxFocuserHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), FakeboxFocuser);
  [fakeboxFocuserHandler fakeboxFocused];
}

#pragma mark - NewTabPageDelegate

- (void)updateFeedLayout {
  // If this coordinator has not finished [self start], the below will start
  // viewDidLoad before the UI is ready, failing DCHECKS.
  if (!self.started) {
    return;
  }
  // TODO(crbug.com/1406940): Investigate why this order is correct. Intuition
  // would be that the layout update should happen before telling UIKit to
  // relayout.
  [self.containedViewController.view setNeedsLayout];
  [self.containedViewController.view layoutIfNeeded];
  [self.NTPViewController updateNTPLayout];
}

- (void)setContentOffsetToTop {
  [self.NTPViewController setContentOffsetToTop];
}

- (BOOL)isGoogleDefaultSearchEngine {
  DCHECK(self.templateURLService);
  const TemplateURL* defaultURL =
      self.templateURLService->GetDefaultSearchProvider();
  BOOL isGoogleDefaultSearchProvider =
      defaultURL &&
      defaultURL->GetEngineType(self.templateURLService->search_terms_data()) ==
          SEARCH_ENGINE_GOOGLE;
  return isGoogleDefaultSearchProvider;
}

- (BOOL)isStartSurface {
  // The web state is nil if the NTP is in another tab. In this case, it is
  // never a start surface.
  if (!self.webState) {
    return NO;
  }
  NewTabPageTabHelper* NTPHelper =
      NewTabPageTabHelper::FromWebState(self.webState);
  return NTPHelper && NTPHelper->ShouldShowStartSurface();
}

- (void)handleFeedTopSectionClosed {
  [self.NTPViewController updateScrollPositionForFeedTopSectionClosed];
}

#pragma mark - NewTabPageFollowDelegate

- (NSUInteger)followedPublisherCount {
  return self.followedWebSites.count;
}

- (BOOL)doesFollowingFeedHaveContent {
  for (FollowedWebSite* web_site in self.followedWebSites) {
    if (web_site.state == FollowedWebSiteStateStateActive) {
      return YES;
    }
  }

  return NO;
}

- (NSArray<FollowedWebSite*>*)followedWebSites {
  FollowBrowserAgent* followBrowserAgent =
      FollowBrowserAgent::FromBrowser(self.browser);

  // Return an empty list if the BrowserAgent is null (which can happen
  // if e.g. the Browser is off-the-record).
  if (!followBrowserAgent)
    return @[];

  return followBrowserAgent->GetFollowedWebSites();
}

#pragma mark - NewTabPageMetricsDelegate

- (void)recentTabTileOpened {
  [self.NTPMetricsRecorder
      recordHomeActionType:IOSHomeActionType::kReturnToRecentTab
            onStartSurface:[self isStartSurface]];
}

- (void)feedArticleOpened {
  [self.NTPMetricsRecorder recordHomeActionType:IOSHomeActionType::kFeedCard
                                 onStartSurface:[self isStartSurface]];
}

- (void)mostVisitedTileOpened {
  [self.NTPMetricsRecorder
      recordHomeActionType:IOSHomeActionType::kMostVisitedTile
            onStartSurface:[self isStartSurface]];
}

- (void)shortcutTileOpened {
  [self.NTPMetricsRecorder recordHomeActionType:IOSHomeActionType::kShortcuts
                                 onStartSurface:[self isStartSurface]];
}

#pragma mark - LogoAnimationControllerOwnerOwner

- (id<LogoAnimationControllerOwner>)logoAnimationControllerOwner {
  return [self.headerViewController logoAnimationControllerOwner];
}

#pragma mark - OverscrollActionsControllerDelegate

- (void)overscrollActionNewTab:(OverscrollActionsController*)controller {
  id<ApplicationCommands> applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationCommandsHandler openURLInNewTab:[OpenNewTabCommand command]];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kOpenedNewTab];
}

- (void)overscrollActionCloseTab:(OverscrollActionsController*)controller {
  id<BrowserCoordinatorCommands> browserCoordinatorCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         BrowserCoordinatorCommands);
  [browserCoordinatorCommandsHandler closeCurrentTab];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kCloseTab];
}

- (void)overscrollActionRefresh:(OverscrollActionsController*)controller {
  [self reload];
  [self.NTPMetricsRecorder
      recordOverscrollActionForType:OverscrollActionType::kPullToRefresh];
}

- (BOOL)shouldAllowOverscrollActionsForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return YES;
}

- (UIView*)toolbarSnapshotViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [[self.headerViewController toolBarView]
      snapshotViewAfterScreenUpdates:NO];
}

- (UIView*)headerViewForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return self.feedWrapperViewController.view;
}

- (CGFloat)headerInsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return [self.NTPViewController heightAboveFeed];
}

- (CGFloat)headerHeightForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  CGFloat height = [self.headerViewController toolBarView].bounds.size.height;
  CGFloat topInset = self.feedWrapperViewController.view.safeAreaInsets.top;
  return height + topInset;
}

- (CGFloat)initialContentOffsetForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  return -[self headerInsetForOverscrollActionsController:controller];
}

- (FullscreenController*)fullscreenControllerForOverscrollActionsController:
    (OverscrollActionsController*)controller {
  // Fullscreen isn't supported here.
  return nullptr;
}

#pragma mark - AppStateObserver

- (void)appState:(AppState*)appState
    didTransitionFromInitStage:(InitStage)previousInitStage {
  if (previousInitStage == InitStageFirstRun) {
    self.NTPViewController.focusAccessibilityOmniboxWhenViewAppears = YES;
    [self.headerViewController focusAccessibilityOnOmnibox];

    [appState removeObserver:self];
  }
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self handleFeedVisibilityDidChange];
}

#pragma mark - DiscoverFeedObserverBridge

- (void)discoverFeedModelWasCreated {
  if (self.NTPViewController.viewDidAppear) {
    [self updateNTPForFeed];

    if (IsWebChannelsEnabled()) {
      [self.feedHeaderViewController updateForFollowingFeedVisibilityChanged];
      [self updateFeedLayout];
    }
    [self.NTPViewController setContentOffsetToTop];
  }
}

#pragma mark - IdentityManagerObserverBridgeDelegate

- (void)onPrimaryAccountChanged:
    (const signin::PrimaryAccountChangeEvent&)event {
  // An account change may trigger after the coordinator has been stopped.
  // In this case do not process the event.
  if (!self.started) {
    return;
  }
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared: {
      [self updateFeedVisibilityForSupervision];
      break;
    }
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (!self.started) {
    return;
  }
  if (preferenceName == prefs::kArticlesForYouEnabled ||
      preferenceName == prefs::kNTPContentSuggestionsEnabled ||
      preferenceName == prefs::kNTPContentSuggestionsForSupervisedUserEnabled) {
    [self updateNTPForFeed];
    [self setContentOffsetToTop];
  }
  if (preferenceName ==
      DefaultSearchManager::kDefaultSearchProviderDataPrefName) {
    [self defaultSearchEngineDidChange];
  }
}

#pragma mark - SceneStateObserver

- (void)sceneState:(SceneState*)sceneState
    transitionedToActivationLevel:(SceneActivationLevel)level {
  self.sceneInForeground = level >= SceneActivationLevelForegroundInactive;
  [self updateVisible];
}

#pragma mark - WebStateListObserving methods

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(ActiveWebStateChangeReason)reason {
  [self didChangeActiveWebState:newWebState];
}

#pragma mark - Private

// Handles a change in the active WebState.
- (void)didChangeActiveWebState:(web::WebState*)newWebState {
  if (self.webState == newWebState) {
    return;
  }

  if (IsNTPActiveForWebState(self.webState)) {
    [self NTPDidChangeVisibility:NO];
  }

  bool active = IsNTPActiveForWebState(newWebState);
  self.webState = active ? newWebState : nullptr;

  if (active) {
    [self NTPDidChangeVisibility:YES];
  }
}

// Updates the feed visibility or content based on the supervision state
// of the account defined in `value`.
- (void)updateFeedWithIsSupervisedUser:(BOOL)value {
  // This may be called asynchronously after the NTP has
  // been stopped and the object has been stopped. Ignore
  // the invocation.
  PrefService* prefService = self.prefService;
  if (!prefService) {
    return;
  }

  prefService->SetBoolean(prefs::kNTPContentSuggestionsForSupervisedUserEnabled,
                          !value);
}

- (void)updateStartForVisibilityChange:(BOOL)visible {
  if (visible && NewTabPageTabHelper::FromWebState(self.webState)
                     ->ShouldShowStartSurface()) {
    [self.contentSuggestionsCoordinator configureStartSurfaceIfNeeded];
  }
  if (!visible && NewTabPageTabHelper::FromWebState(self.webState)
                      ->ShouldShowStartSurface()) {
    // This means the NTP going away was showing Start. Reset configuration
    // since it should not show Start after disappearing.
    NewTabPageTabHelper::FromWebState(self.webState)
        ->SetShowStartSurface(false);
  }
}

// Updates the visible property based on viewPresented and sceneInForeground
// properties.
// Sends metrics when NTP becomes invisible.
- (void)updateVisible {
  BOOL visible = self.viewPresented && self.sceneInForeground;
  if (visible == self.visible) {
    return;
  }
  self.visible = visible;
  if (self.browser->GetBrowserState()->IsOffTheRecord()) {
    // Do not report metrics on incognito NTP.
    return;
  }
  if (visible) {
    self.didAppearTime = base::TimeTicks::Now();
    if ([self isFeedHeaderVisible]) {
      if ([self.feedExpandedPref value]) {
        [self.NTPMetricsRecorder
            recordNTPImpression:IOSNTPImpressionType::kFeedVisible];
      } else {
        [self.NTPMetricsRecorder
            recordNTPImpression:IOSNTPImpressionType::kFeedCollapsed];
      }
    } else {
      [self.NTPMetricsRecorder
          recordNTPImpression:IOSNTPImpressionType::kFeedDisabled];
    }
  } else {
    if (!self.didAppearTime.is_null()) {
      [self.NTPMetricsRecorder
          recordTimeSpentInNTP:base::TimeTicks::Now() - self.didAppearTime];
      self.didAppearTime = base::TimeTicks();
    }
  }
}

// Updates the NTP to take into account a new feed, or a change in feed
// visibility.
- (void)updateNTPForFeed {
  DCHECK(self.NTPViewController);

  if (!self.started) {
    return;
  }

  [self.NTPViewController resetViewHierarchy];

  if (self.feedViewController) {
    self.discoverFeedService->RemoveFeedViewController(self.feedViewController);
  }

  [self.feedTopSectionCoordinator stop];

  self.NTPViewController.feedWrapperViewController = nil;
  self.NTPViewController.feedTopSectionViewController = nil;
  self.feedWrapperViewController = nil;
  self.feedViewController = nil;
  self.feedTopSectionCoordinator = nil;

  // Fetches feed header and conditionally fetches feed. Feed can only be
  // visible if feed header is visible.
  if ([self isFeedHeaderVisible]) {
    [self configureFeedAndHeader];
  } else {
    self.NTPViewController.feedHeaderViewController = nil;
    self.feedHeaderViewController = nil;
  }

  if ([self isFeedTopSectionVisible]) {
    self.NTPViewController.feedTopSectionViewController =
        self.feedTopSectionCoordinator.viewController;
  }

  self.NTPViewController.feedVisible = [self isFeedVisible];

  self.feedWrapperViewController = [[FeedWrapperViewController alloc]
        initWithDelegate:self
      feedViewController:self.feedViewController];

  self.NTPViewController.feedWrapperViewController =
      self.feedWrapperViewController;

  [self.NTPViewController layoutContentInParentCollectionView];

  [self updateFeedLayout];
}

// Feed header is always visible unless it is disabled (eg. Disabled from Chrome
// settings, enterprise policy, safe mode, etc.).
- (BOOL)isFeedHeaderVisible {
  // Feed is disabled in safe mode.
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  BOOL isSafeMode = [sceneState.appState resumingFromSafeMode];

  return self.prefService->GetBoolean(prefs::kArticlesForYouEnabled) &&
         self.prefService->GetBoolean(prefs::kNTPContentSuggestionsEnabled) &&
         !IsFeedAblationEnabled() &&
         IsContentSuggestionsForSupervisedUserEnabled(self.prefService) &&
         !isSafeMode;
}

// Returns `YES` if the feed is currently visible on the NTP.
- (BOOL)isFeedVisible {
  return [self shouldFeedBeVisible] && self.feedViewController;
}

// Whether the feed top section, which contains all content between the feed
// header and the feed, is currently visible.
// TODO(crbug.com/1331010): The feed top section may include content that is not
// the signin promo, which may need to be visible when the user is signed in.
- (BOOL)isFeedTopSectionVisible {
  return IsDiscoverFeedTopSyncPromoEnabled() && [self isFeedVisible] &&
         self.authService &&
         !self.authService->HasPrimaryIdentity(signin::ConsentLevel::kSignin);
}

// Creates, configures and returns a feed view controller configuration.
- (DiscoverFeedViewControllerConfiguration*)feedViewControllerConfiguration {
  DiscoverFeedViewControllerConfiguration* viewControllerConfig =
      [[DiscoverFeedViewControllerConfiguration alloc] init];
  viewControllerConfig.browser = self.browser;
  viewControllerConfig.scrollDelegate = self.NTPViewController;
  viewControllerConfig.previewDelegate = self;
  viewControllerConfig.signInPromoDelegate = self;

  return viewControllerConfig;
}

// Updates the visibility of the content suggestions on the NTP if the account
// is subject to parental controls.
- (void)updateFeedVisibilityForSupervision {
  DCHECK(self.prefService);
  DCHECK(self.authService);

  id<SystemIdentity> identity =
      self.authService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (!identity) {
    [self updateFeedWithIsSupervisedUser:NO];
    return;
  }

  using CapabilityResult = SystemIdentityCapabilityResult;

  __weak NewTabPageCoordinator* weakSelf = self;
  GetApplicationContext()
      ->GetSystemIdentityManager()
      ->IsSubjectToParentalControls(
          identity, base::BindOnce(^(CapabilityResult result) {
            const bool isSupervisedUser = result == CapabilityResult::kTrue;
            [weakSelf updateFeedWithIsSupervisedUser:isSupervisedUser];
          }));
}

// Handles how the NTP reacts when the default search engine is changed.
- (void)defaultSearchEngineDidChange {
  [self.feedHeaderViewController updateForDefaultSearchEngineChanged];
  [self updateFeedLayout];
  [self.NTPViewController setContentOffsetToTop];
}

// Toggles feed visibility between hidden or expanded using the feed header
// menu. A hidden feed will continue to show the header, with a modified label.
// TODO(crbug.com/1304382): Modify this comment when Web Channels is launched.
- (void)setFeedVisibleFromHeader:(BOOL)visible {
  [self.feedExpandedPref setValue:visible];
  [self.feedMetricsRecorder recordDiscoverFeedVisibilityChanged:visible];
  [self handleFeedVisibilityDidChange];
}

// Configures and returns the feed top section coordinator.
- (FeedTopSectionCoordinator*)createFeedTopSectionCoordinator {
  DCHECK(self.NTPViewController);
  FeedTopSectionCoordinator* feedTopSectionCoordinator =
      [[FeedTopSectionCoordinator alloc]
          initWithBaseViewController:self.NTPViewController
                             browser:self.browser];
  feedTopSectionCoordinator.ntpDelegate = self;
  [feedTopSectionCoordinator start];
  return feedTopSectionCoordinator;
}

- (void)handleFeedManageTapped {
  [self.feedMetricsRecorder recordHeaderMenuManageTapped];
  [self.feedManagementCoordinator stop];
  self.feedManagementCoordinator = nil;

  self.feedManagementCoordinator = [[FeedManagementCoordinator alloc]
      initWithBaseViewController:self.NTPViewController
                         browser:self.browser];
  self.feedManagementCoordinator.navigationDelegate = self;
  self.feedManagementCoordinator.feedMetricsRecorder = self.feedMetricsRecorder;
  [self.feedManagementCoordinator start];
}

// Handles how the NTP should react when the feed visbility preference is
// changed.
- (void)handleFeedVisibilityDidChange {
  [self updateNTPForFeed];
  [self setContentOffsetToTop];
  [self.feedHeaderViewController updateForFeedVisibilityChanged];
}

// Private setter for the `webState` property.
- (void)setWebState:(web::WebState*)webState {
  if (_webState == webState) {
    return;
  }

  _webState = webState;
  self.NTPMediator.webState = _webState;
  self.contentSuggestionsCoordinator.webState = _webState;
}

// Called when the NTP changes visibility, either when the user navigates to
// or away from the NTP, or when the active WebState changes.
- (void)NTPDidChangeVisibility:(BOOL)visible {
  DCHECK(self.started);
  DCHECK(self.webState);

  if (!self.browser->GetBrowserState()->IsOffTheRecord()) {
    [self updateStartForVisibilityChange:visible];

    if (visible) {
      if ([self isFollowingFeedAvailable]) {
        NewTabPageTabHelper* helper =
            NewTabPageTabHelper::FromWebState(self.webState);
        self.shouldScrollIntoFeed = helper->GetNextNTPScrolledToFeed();
        [self selectFeedType:helper->GetNextNTPFeedType()];
        helper->SetNextNTPFeedType(NewTabPageTabHelper::DefaultFeedType());

        self.NTPViewController.shouldScrollIntoFeed = self.shouldScrollIntoFeed;
        // Reassign the sort type in case it changed in another tab.
        self.feedHeaderViewController.followingFeedSortType =
            self.followingFeedSortType;
        // Update the header so that it's synced with the currently selected
        // feed, which could have been changed when a new web state was
        // inserted.
        [self.feedHeaderViewController updateForSelectedFeed];
        self.feedMetricsRecorder.feedControlDelegate = self;
        self.feedMetricsRecorder.followDelegate = self;
      }
    }
    // Check if feed is visible before reporting NTP visibility as the feed
    // needs to be visible in order to use for metrics.
    // TODO(crbug.com/1373650) Move isFeedVisible check to the metrics recorder
    if ([self isFeedVisible]) {
      [self.feedMetricsRecorder recordNTPDidChangeVisibility:visible];
    }
  }

  self.viewPresented = visible;
  [self updateVisible];

  if (!self.browser->GetBrowserState()->IsOffTheRecord() && !visible) {
    // Unfocus omnibox, to prevent it from lingering when it should be
    // dismissed (for example, when navigating away or when changing feed
    // visibility).
    // Do this after updating `viewPresented` to prevent defocus animation from
    // happening when already navigating away from NTP.
    [self cancelOmniboxEdit];
  }
}

- (BOOL)isSignInAllowed {
  AuthenticationService::ServiceStatus statusService =
      self.authService->GetServiceStatus();
  switch (statusService) {
    case AuthenticationService::ServiceStatus::SigninDisabledByPolicy:
    case AuthenticationService::ServiceStatus::SigninDisabledByInternal:
    case AuthenticationService::ServiceStatus::SigninDisabledByUser: {
      return NO;
    }
    case AuthenticationService::ServiceStatus::SigninForcedByPolicy:
    case AuthenticationService::ServiceStatus::SigninAllowed: {
      break;
    }
  }
  return YES;
}

- (BOOL)isSyncAllowed {
  if (self.prefService->FindPreference(policy::key::kSyncDisabled) &&
      self.prefService->GetBoolean(policy::key::kSyncDisabled)) {
    return NO;
  }

  return YES;
}

- (void)showSignInDisableMessage {
  id<SnackbarCommands> handler =
      static_cast<id<SnackbarCommands>>(self.browser->GetCommandDispatcher());
  MDCSnackbarMessage* message = [MDCSnackbarMessage
      messageWithText:
          l10n_util::GetNSString(
              IDS_IOS_NTP_FEED_SIGNIN_PROMO_DISABLE_SNACKBAR_MESSAGE)];

  [handler showSnackbarMessage:message];
}

#pragma mark - Getters

- (FeedHeaderViewController*)feedHeaderViewController {
  DCHECK(!self.browser->GetBrowserState()->IsOffTheRecord());
  if (!_feedHeaderViewController) {
    BOOL followingSegmentDotVisible = NO;
    if (IsDotEnabledForNewFollowedContent() && IsWebChannelsEnabled()) {
      // Only show the dot if the user follows available publishers.
      followingSegmentDotVisible =
          [self doesFollowingFeedHaveContent] &&
          self.discoverFeedService->GetFollowingFeedHasUnseenContent() &&
          self.selectedFeed != FeedTypeFollowing;
    }
    _feedHeaderViewController = [[FeedHeaderViewController alloc]
        initWithFollowingFeedSortType:self.followingFeedSortType
           followingSegmentDotVisible:followingSegmentDotVisible];
    _feedHeaderViewController.feedControlDelegate = self;
    _feedHeaderViewController.ntpDelegate = self;
    _feedHeaderViewController.feedMetricsRecorder = self.feedMetricsRecorder;
    [_feedHeaderViewController.menuButton
               addTarget:self
                  action:@selector(openFeedMenu)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return _feedHeaderViewController;
}

@end
