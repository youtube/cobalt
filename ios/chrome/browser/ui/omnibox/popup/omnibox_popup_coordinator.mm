// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_coordinator.h"

#import "base/feature_list.h"
#import "components/favicon/core/large_icon_service.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/history/core/browser/top_sites.h"
#import "components/image_fetcher/core/image_data_fetcher.h"
#import "components/omnibox/browser/autocomplete_result.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/autocomplete/remote_suggestions_service_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/favicon/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_cache_factory.h"
#import "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/flags/system_flags.h"
#import "ios/chrome/browser/history/top_sites_factory.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ntp/new_tab_page_util.h"
#import "ios/chrome/browser/policy/policy_util.h"
#import "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state_browser_agent.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/ui/favicon/favicon_attributes_provider.h"
#import "ios/chrome/browser/ui/main/default_browser_scene_agent.h"
#import "ios/chrome/browser/ui/menu/browser_action_factory.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel_item.h"
#import "ios/chrome/browser/ui/omnibox/popup/carousel_item_menu_provider.h"
#import "ios/chrome/browser/ui/omnibox/popup/content_providing.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_pedal_annotator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_container_view.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_mediator.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_presenter.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_ios.h"
#import "ios/chrome/browser/ui/omnibox/popup/pedal_section_extractor.h"
#import "ios/chrome/browser/ui/omnibox/popup/popup_debug_info_view_controller.h"
#import "ios/chrome/browser/ui/sharing/sharing_coordinator.h"
#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/device_form_factor.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface OmniboxPopupCoordinator () <OmniboxPopupMediatorProtocolProvider,
                                       OmniboxPopupMediatorSharingDelegate> {
  std::unique_ptr<OmniboxPopupViewIOS> _popupView;
}

@property(nonatomic, strong) OmniboxPopupViewController* popupViewController;
@property(nonatomic, strong) OmniboxPopupMediator* mediator;
@property(nonatomic, strong) SharingCoordinator* sharingCoordinator;

// Owned by OmniboxEditModel.
@property(nonatomic, assign) AutocompleteController* autocompleteController;

@end

@implementation OmniboxPopupCoordinator

#pragma mark - Public

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
        autocompleteController:(AutocompleteController*)autocompleteController
                     popupView:(std::unique_ptr<OmniboxPopupViewIOS>)popupView {
  self = [super initWithBaseViewController:nil browser:browser];
  if (self) {
    DCHECK(autocompleteController);
    _autocompleteController = autocompleteController;
    _popupView = std::move(popupView);
    _popupViewController = [[OmniboxPopupViewController alloc] init];
    _popupReturnDelegate = _popupViewController;
    _KeyboardDelegate = _popupViewController;
  }
  return self;
}

- (void)start {
  std::unique_ptr<image_fetcher::ImageDataFetcher> imageFetcher =
      std::make_unique<image_fetcher::ImageDataFetcher>(
          self.browser->GetBrowserState()->GetSharedURLLoaderFactory());

  BOOL isIncognito = self.browser->GetBrowserState()->IsOffTheRecord();

  RemoteSuggestionsService* remoteSuggestionsService =
      RemoteSuggestionsServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState(), /*create_if_necessary=*/true);

  self.mediator = [[OmniboxPopupMediator alloc]
               initWithFetcher:std::move(imageFetcher)
                 faviconLoader:IOSChromeFaviconLoaderFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())
        autocompleteController:self.autocompleteController
      remoteSuggestionsService:remoteSuggestionsService
                      delegate:_popupView.get()
                       tracker:feature_engagement::TrackerFactory::
                                   GetForBrowserState(
                                       self.browser->GetBrowserState())];
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.mediator.dispatcher =
      static_cast<id<BrowserCommands>>(self.browser->GetCommandDispatcher());

  self.mediator.webStateList = self.browser->GetWebStateList();
  TemplateURLService* templateURLService =
      ios::TemplateURLServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator.defaultSearchEngineIsGoogle =
      templateURLService && templateURLService->GetDefaultSearchProvider() &&
      templateURLService->GetDefaultSearchProvider()->GetEngineType(
          templateURLService->search_terms_data()) == SEARCH_ENGINE_GOOGLE;
  self.mediator.protocolProvider = self;
  self.mediator.sharingDelegate = self;
  BrowserActionFactory* actionFactory = [[BrowserActionFactory alloc]
      initWithBrowser:self.browser
             scenario:MenuScenarioHistogram::kOmniboxMostVisitedEntry];
  self.mediator.mostVisitedActionFactory = actionFactory;
  self.popupViewController.imageRetriever = self.mediator;
  self.popupViewController.faviconRetriever = self.mediator;
  self.popupViewController.delegate = self.mediator;
  self.popupViewController.dataSource = self.mediator;
  self.popupViewController.incognito = isIncognito;
  favicon::LargeIconService* largeIconService =
      IOSChromeLargeIconServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  LargeIconCache* cache = IOSChromeLargeIconCacheFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.popupViewController.largeIconService = largeIconService;
  self.popupViewController.largeIconCache = cache;
  self.popupViewController.carouselMenuProvider = self.mediator;
  self.popupViewController.layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);

  self.mediator.consumer = self.popupViewController;
  self.popupViewController.matchPreviewDelegate =
      self.popupMatchPreviewDelegate;
  self.popupViewController.acceptReturnDelegate = self.acceptReturnDelegate;
  self.mediator.carouselItemConsumer = self.popupViewController;
  self.mediator.allowIncognitoActions =
      !IsIncognitoModeDisabled(self.browser->GetBrowserState()->GetPrefs());

  OmniboxPedalAnnotator* annotator = [[OmniboxPedalAnnotator alloc] init];
  annotator.pedalsEndpoint = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  annotator.omniboxCommandHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), OmniboxCommands);
  self.mediator.pedalAnnotator = annotator;

  self.mediator.applicationCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  self.mediator.incognito = isIncognito;
  SceneState* sceneState =
      SceneStateBrowserAgent::FromBrowser(self.browser)->GetSceneState();
  self.mediator.promoScheduler =
      [DefaultBrowserSceneAgent agentFromScene:sceneState].nonModalScheduler;
  self.mediator.presenter = [[OmniboxPopupPresenter alloc]
      initWithPopupPresenterDelegate:self.presenterDelegate
                 popupViewController:self.popupViewController
                           incognito:isIncognito];

  _popupView->SetMediator(self.mediator);

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [self setupDebug];
  }
}

- (void)stop {
  [self.sharingCoordinator stop];
  _popupView.reset();
}

- (BOOL)isOpen {
  return self.mediator.isOpen;
}

#pragma mark - Property accessor

- (BOOL)hasResults {
  return self.mediator.hasResults;
}

#pragma mark - OmniboxPopupMediatorProtocolProvider

- (scoped_refptr<history::TopSites>)topSites {
  return ios::TopSitesFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

- (id<SnackbarCommands>)snackbarCommandsHandler {
  return HandlerForProtocol(self.browser->GetCommandDispatcher(),
                            SnackbarCommands);
}

#pragma mark - OmniboxPopupMediatorSharingDelegate

/// Triggers the URL sharing flow for the given `URL` and `title`, with the
/// origin `view` representing the UI component for that URL.
- (void)popupMediator:(OmniboxPopupMediator*)mediator
             shareURL:(GURL)URL
                title:(NSString*)title
           originView:(UIView*)originView {
  SharingParams* params = [[SharingParams alloc]
      initWithURL:URL
            title:title
         scenario:SharingScenario::OmniboxMostVisitedEntry];
  self.sharingCoordinator = [[SharingCoordinator alloc]
      initWithBaseViewController:self.popupViewController
                         browser:self.browser
                          params:params
                      originView:originView];
  [self.sharingCoordinator start];
}

#pragma mark - private

- (void)setupDebug {
  DCHECK(experimental_flags::IsOmniboxDebuggingEnabled());

  PopupDebugInfoViewController* viewController =
      [[PopupDebugInfoViewController alloc] init];
  self.mediator.debugInfoConsumer = viewController;

  UINavigationController* navController = [[UINavigationController alloc]
      initWithRootViewController:viewController];
  self.popupViewController.debugInfoViewController = navController;
}

@end
