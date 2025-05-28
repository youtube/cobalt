// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/profile/profile_state.h"
#import "ios/chrome/browser/context_menu/ui_bundled/context_menu_configuration_provider.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_controller.h"
#import "ios/chrome/browser/lens/ui_bundled/lens_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_result_page_mediator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_configuration_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_entrypoint.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_metrics_recorder.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_overflow_menu_factory.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_pan_tracker.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"
#import "ios/chrome/browser/lens_overlay/model/snapshot_cover_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_presenter.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_consent_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_presenter.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_network_issue_alert_presenter.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_results_page_presenter_delegate.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_consumer.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_result_page_view_controller.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/menu/ui_bundled/browser_action_factory.h"
#import "ios/chrome/browser/omnibox/ui_bundled/chrome_omnibox_client_ios.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_coordinator.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_focus_delegate.h"
#import "ios/chrome/browser/search_engines/model/template_url_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/chrome/browser/web/model/web_state_delegate_browser_agent.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {

// The expected number of animations happening at the same time when exiting.
const int kExpectedExitAnimationCount = 2;

// The delay for showing the search with camera tooltip hint.
const base::TimeDelta kSearchWithCameraTooltipHintDelay = base::Seconds(2.0);

}  // namespace

@interface LensOverlayCoordinator () <LensOverlayConsentPresenterDelegate,
                                      LensOverlayCommands,
                                      LensOverlayMediatorDelegate,
                                      LensOverlayResultConsumer,
                                      LensOverlayConsentViewControllerDelegate,
                                      LensOverlayNetworkIssueDelegate,
                                      LensOverlayResultsPagePresenterDelegate>

// Whether the `_containerViewController` is currently presented.
@property(nonatomic, assign, readonly) BOOL isLensOverlayVisible;

// Whether the UI is created.
@property(nonatomic, assign, readonly) BOOL isUICreated;

@end

@implementation LensOverlayCoordinator {
  /// Container view controller.
  /// Hosts all of lens UI: contains the selection UI, presents the results UI
  /// modally.
  LensOverlayContainerViewController* _containerViewController;

  /// The mediator for lens overlay.
  LensOverlayMediator* _mediator;

  /// The view controller for lens results.
  LensResultPageViewController* _resultViewController;
  /// The mediator for lens results.
  LensResultPageMediator* _resultMediator;
  /// The context menu configuration provider for the result page.
  ContextMenuConfigurationProvider* _resultContextMenuProvider;

  /// The tab helper associated with the current UI.
  base::WeakPtr<LensOverlayTabHelper> _associatedTabHelper;

  /// Coordinator of the omnibox.
  OmniboxCoordinator* _omniboxCoordinator;

  LensOverlayConsentViewController* _consentViewController;

  UIViewController<ChromeLensOverlay>* _selectionViewController;

  /// Indicates the Lens Overlay is in the exit flow.
  BOOL _isExiting;

  /// Indicates this coordinator has received the `stop` call.
  BOOL _isStopped;

  /// This auxiliary window is used while restoring the sheet state when
  /// returning to the tab where Lens Overlay is active.
  UIWindow* _restorationWindow;

  /// A helper object that provides a central point for recording metrics.
  LensOverlayMetricsRecorder* _metricsRecorder;

  /// Consent dialog presenter.
  LensOverlayConsentPresenter* _lensOverlayConsentPresenter;

  /// Network issue alert presenter.
  LensOverlayNetworkIssueAlertPresenter* _networkIssueAlertPresenter;

  /// Presenter for the results page.
  LensOverlayResultsPagePresenter* _resultsPagePresenter;

  /// Presenter for the lens container.
  LensOverlayContainerPresenter* _containerPresenter;

  // The entrypoint associated with the current session.
  LensOverlayEntrypoint _entrypoint;
}

#pragma mark - public

- (UIViewController*)viewController {
  return _containerViewController;
}

#pragma mark - Helpers

// Returns whether the UI was created succesfully.
- (BOOL)createUIWithImageSource:(LensImageSource*)imageSource {
  [self createContainerViewController];
  [self createSelectionUIWithImageSource:imageSource];
  if (!_selectionViewController) {
    return NO;
  }

  [self createMediator];

  // Wire up consumers and delegates
  _containerViewController.selectionViewController = _selectionViewController;
  [_selectionViewController setLensOverlayDelegate:_mediator];
  _mediator.lensHandler = _selectionViewController;
  _mediator.commandsHandler = self;
  _mediator.delegate = self;
  // The mediator might destroy lens UI if the search engine doesn't support
  // lens.
  _mediator.templateURLService =
      ios::TemplateURLServiceFactory::GetForProfile(self.browser->GetProfile());

  _networkIssueAlertPresenter = [[LensOverlayNetworkIssueAlertPresenter alloc]
      initWithBaseViewController:_containerViewController];
  _networkIssueAlertPresenter.delegate = self;

  return YES;
}

- (void)createSelectionUIWithImageSource:(LensImageSource*)imageSource {
  if (_selectionViewController) {
    return;
  }

  LensOverlayConfigurationFactory* lensConfigurationFactory =
      [[LensOverlayConfigurationFactory alloc] init];
  LensConfiguration* config = [lensConfigurationFactory
      configurationForEntrypoint:_entrypoint
                         profile:self.browser->GetProfile()];

  LensOverlayOverflowMenuFactory* overflowMenuFactory =
      [[LensOverlayOverflowMenuFactory alloc]
                            initWithBrowser:self.browser
          browserCoordinatorCommandsHandler:HandlerForProtocol(
                                                self.browser
                                                    ->GetCommandDispatcher(),
                                                BrowserCoordinatorCommands)];

  BOOL escapeHatchEnabled = IsLVFEscapeHatchEnabled();
  config.useTrailingDismissButton = !escapeHatchEnabled;

  NSArray<UIAction*>* additionalMenuItems;
  if (escapeHatchEnabled) {
    __weak __typeof(self) weakSelf = self;
    UIAction* searchWithCameraAction =
        [overflowMenuFactory searchWithCameraActionWithHandler:^{
          [weakSelf didRequestSearchWithCamera];
        }];
    additionalMenuItems = @[
      searchWithCameraAction,
      [overflowMenuFactory openUserActivityAction],
      [overflowMenuFactory learnMoreAction],
    ];
  } else {
    additionalMenuItems = @[
      [overflowMenuFactory openUserActivityAction],
      [overflowMenuFactory learnMoreAction],
    ];
  }

  _selectionViewController = ios::provider::NewChromeLensOverlay(
      imageSource, config, additionalMenuItems);
}

- (void)createContainerViewController {
  if (_containerViewController) {
    return;
  }
  _containerViewController = [[LensOverlayContainerViewController alloc]
      initWithLensOverlayCommandsHandler:self];
}

- (void)createMediator {
  if (_mediator) {
    return;
  }
  Browser* browser = self.browser;
  _mediator = [[LensOverlayMediator alloc]
      initWithIsIncognito:browser->GetProfile()->IsOffTheRecord()];
  _mediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);

  // Results UI is lazily initialized; see comment in LensOverlayResultConsumer
  // section.
  _mediator.resultConsumer = self;
}

- (BOOL)createConsentViewController {
  _consentViewController = [[LensOverlayConsentViewController alloc] init];
  _consentViewController.delegate = self;

  return YES;
}

#pragma mark - ChromeCoordinator

- (void)start {
  CHECK(IsLensOverlayAvailable());
  [super start];

  Browser* browser = self.browser;
  CHECK(browser, kLensOverlayNotFatalUntil);

  [browser->GetCommandDispatcher()
      startDispatchingToTarget:self
                   forProtocol:@protocol(LensOverlayCommands)];
}

- (void)stop {
  _isStopped = YES;

  if (Browser* browser = self.browser) {
    [browser->GetCommandDispatcher() stopDispatchingToTarget:self];
  }
  [self destroyLensUI:NO reason:lens::LensOverlayDismissalSource::kTabClosed];

  [super stop];
}

#pragma mark - LensOverlayCommands

- (void)searchWithLensImageMetadata:(id<LensImageMetadata>)metadata
                         entrypoint:(LensOverlayEntrypoint)entrypoint
                         completion:(void (^)(BOOL))completion {
  [self prepareOverlayWithEntrypoint:entrypoint];
  // Even if the image is already prepared at this point, the snapshotting
  // infrastructure still needs to be built to allow the restoration window to
  // be displayed when exiting and re-entering the experience.
  [self prepareSnapshotCapturingInfrastructure];
  LensImageSource* imageSource =
      [[LensImageSource alloc] initWithImageMetadata:metadata];
  [self handleOverlayImageSourceFound:imageSource
                             animated:YES
                           completion:completion];
}

- (void)searchImageWithLens:(UIImage*)image
                 entrypoint:(LensOverlayEntrypoint)entrypoint
                 completion:(void (^)(BOOL))completion {
  [self prepareOverlayWithEntrypoint:entrypoint];
  // Even if the image is already prepared at this point, the snapshotting
  // infrastructure still needs to be built to allow the restoration window to
  // be displayed when exiting and re-entering the experience.
  [self prepareSnapshotCapturingInfrastructure];
  LensImageSource* imageSource =
      [[LensImageSource alloc] initWithSnapshot:image];
  [self handleOverlayImageSourceFound:imageSource
                             animated:YES
                           completion:completion];
}

- (void)createAndShowLensUI:(BOOL)animated
                 entrypoint:(LensOverlayEntrypoint)entrypoint
                 completion:(void (^)(BOOL))completion {
  [self prepareOverlayWithEntrypoint:entrypoint];
  __weak __typeof(self) weakSelf = self;
  [self captureSnapshotWithCompletion:^(UIImage* snapshot) {
    LensImageSource* imageSource =
        [[LensImageSource alloc] initWithSnapshot:snapshot];
    [weakSelf handleOverlayImageSourceFound:imageSource
                                   animated:animated
                                 completion:completion];
  }];
}

// Handles presenting the base image to be used in the overlay.
- (void)handleOverlayImageSourceFound:(LensImageSource*)imageSource
                             animated:(BOOL)animated
                           completion:(void (^)(BOOL))completion {
  if (!imageSource.isValid) {
    if (completion) {
      completion(NO);
    }
    return;
  }

  BOOL success = [self createUIWithImageSource:imageSource];
  if (success) {
    [self showLensUI:animated completion:completion];
  } else {
    [self destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::
                            kErrorScreenshotCreationFailed];
    if (completion) {
      completion(NO);
    }
  }
}

- (void)showLensUI:(BOOL)animated {
  [self showLensUI:animated completion:nil];
}

- (void)showLensUI:(BOOL)animated completion:(void (^)(BOOL))completion {
  if (!self.isUICreated || self.isLensOverlayVisible) {
    return;
  }

  [_selectionViewController setTopIconsHidden:self.shouldShowConsentFlow];

  [_metricsRecorder setLensOverlayInForeground:YES];

  [self showRestorationWindowIfNeeded];

  _containerPresenter = [[LensOverlayContainerPresenter alloc]
      initWithBaseViewController:self.baseViewController
         containerViewController:_containerViewController];

  __weak __typeof(self) weakSelf = self;
  [_containerPresenter
      presentContainerAnimated:animated
                    sceneState:self.browser->GetSceneState()
                    completion:^{
                      if (completion) {
                        completion(YES);
                      }
                      [weakSelf onContainerViewControllerPresented];
                    }];
}

- (void)onContainerViewControllerPresented {
  // In some situations this coordinator shouldn't do anything because it's
  // already being torn down. Just do minimal clean up and return.
  if (_isStopped || _isExiting) {
    if (_associatedTabHelper) {
      _associatedTabHelper->ReleaseSnapshotAuxiliaryWindows();
    }
    return;
  }

  if (self.shouldShowConsentFlow) {
    if (self.isResultsBottomSheetCreated) {
      [self stopResultPage];
    }
    [self presentConsentFlow];
  } else {
    // Start the selection UI only when the container is presented. This avoids
    // results being reported before the container is fully shown.
    [_selectionViewController start];

    if (self.isResultsBottomSheetCreated) {
      // Only show the bottom sheet when in selection. For translate, build the
      // necessary infrastructure but don't show it, effectively starting it
      // hidden.
      [self buildResultsBottomSheetPresentation];
      if (!_selectionViewController.translateFilterActive) {
        [self showResultsBottomSheet];
      }
    }
  }

  // The auxiliary window should be retained until the container is confirmed
  // presented to avoid visual flickering when swapping back the main window.
  if (_associatedTabHelper) {
    _associatedTabHelper->ReleaseSnapshotAuxiliaryWindows();
  }

  // If the results bottom sheet hasn't been created yet, dismiss the
  // restoration window. Otherwise, keep the restoration window until the
  // results bottom sheet is presented.
  if (!self.isResultsBottomSheetCreated) {
    [self dismissRestorationWindow];
  }
}

- (void)presentConsentFlow {
  [self createConsentViewController];
  [_metricsRecorder recordLensOverlayConsentShown];
  [self disableSelectionInteraction:YES];

  _lensOverlayConsentPresenter = [[LensOverlayConsentPresenter alloc]
      initWithPresentingViewController:_containerViewController
        presentedConsentViewController:_consentViewController];
  _lensOverlayConsentPresenter.delegate = self;
  [_lensOverlayConsentPresenter showConsentViewController];

  [_metricsRecorder recordPermissionRequestedToBeShown];
}

- (void)hideLensUI:(BOOL)animated completion:(void (^)())completion {
  if (!self.isUICreated) {
    return;
  }

  _resultsPagePresenter.delegate = nil;
  [_metricsRecorder setLensOverlayInForeground:NO];
  if (_associatedTabHelper) {
    _associatedTabHelper->UpdateSnapshotStorage();
  }

  [self dismissRestorationWindow];
  __weak id<LensCommands> weakCommands =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  [weakCommands lensOverlayWillDismissWithCause:
                    LensOverlayDismissalCauseExternalNavigation];
  __weak LensOverlayResultsPagePresenter* weakResultsPagePresenter =
      _resultsPagePresenter;

  [_containerPresenter
      dismissContainerAnimated:animated
                    completion:^{
                      [weakCommands
                          lensOverlayDidDismissWithCause:
                              LensOverlayDismissalCauseExternalNavigation];
                      // If the result page is still present, dismiss it before
                      // calling the completion.
                      if (weakResultsPagePresenter) {
                        [weakResultsPagePresenter
                            dismissResultsPageAnimated:animated
                                            completion:completion];
                      } else if (completion) {
                        completion();
                      }
                    }];
}

- (void)destroyLensUI:(BOOL)animated
               reason:(lens::LensOverlayDismissalSource)dismissalSource {
  if (_isExiting) {
    return;
  }

  _isExiting = YES;

  [[NSNotificationCenter defaultCenter]
      removeObserver:self
                name:UIApplicationDidReceiveMemoryWarningNotification
              object:nil];

  [_metricsRecorder
      recordDismissalMetricsWithSource:dismissalSource
                     generatedTabCount:_mediator.generatedTabCount];

  // The reason the UI is destroyed can be that Omnient gets associated to a
  // different tab. In this case mark the stale tab helper as not shown.
  if (_associatedTabHelper) {
    _associatedTabHelper->SetLensOverlayUIAttachedAndAlive(false);
    _associatedTabHelper->RecordSheetDimensionState(SheetDimensionStateHidden);
    _associatedTabHelper->ClearViewportSnapshot();
    _associatedTabHelper->UpdateSnapshot();
    if (IsLensOverlaySameTabNavigationEnabled()) {
      _associatedTabHelper->ClearInvokationNavigationId();
    }
  }

  if (!animated) {
    [self exitWithoutAnimation];
    return;
  }

  // Taking the screenshot triggered fullscreen mode. Ensure it's reverted in
  // the cleanup process. Exiting fullscreen has to happen on destruction to
  // ensure a smooth transition back to the content.
  __weak __typeof(self) weakSelf = self;
  __weak id<LensCommands> weakCommands =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);

  BOOL dismissedWithSwipeDown =
      dismissalSource ==
      lens::LensOverlayDismissalSource::kBottomSheetDismissed;

  BOOL isInTranslate = _selectionViewController.translateFilterActive;

  LensOverlayDismissalCause dismissalCause;
  if (dismissedWithSwipeDown) {
    if (isInTranslate) {
      dismissalCause = LensOverlayDismissalCauseSwipeDownFromTranslate;
    } else {
      dismissalCause = LensOverlayDismissalCauseSwipeDownFromSelection;
    }
  } else {
    dismissalCause = LensOverlayDismissalCauseDismissButton;
  }

  [weakCommands lensOverlayWillDismissWithCause:dismissalCause];
  void (^onAnimationFinished)() = ^{
    [weakSelf dismissLensOverlayWithCompletion:^{
      [weakCommands lensOverlayDidDismissWithCause:dismissalCause];
      [weakSelf destroyViewControllersAndMediators];
    }];
  };

  [self executeExitAnimationFlowWithCompletion:onAnimationFinished];
}

#pragma mark - Exit animations

- (void)exitWithoutAnimation {
  __weak __typeof(self) weakSelf = self;
  [_containerPresenter
      dismissContainerAnimated:NO
                    completion:^{
                      [weakSelf exitFullscreenAnimated:NO];
                      [weakSelf destroyViewControllersAndMediators];
                    }];
}

- (void)executeExitAnimationFlowWithCompletion:(void (^)())completion {
  __block int completionCount = 0;
  void (^onAnimationFinished)() = ^{
    completionCount++;
    if (completionCount == kExpectedExitAnimationCount) {
      if (completion) {
        completion();
      }
    }
  };

  [self animateBottomSheetExitWithCompletion:onAnimationFinished];
  [self animateSelectionUIExitWithCompletion:onAnimationFinished];
}

- (void)animateBottomSheetExitWithCompletion:(void (^)())completion {
  if (_lensOverlayConsentPresenter.isConsentVisible) {
    [_lensOverlayConsentPresenter
        dismissConsentViewControllerAnimated:YES
                                  completion:completion];
    return;
  }

  if (_resultsPagePresenter.isResultPageVisible) {
    [_resultsPagePresenter dismissResultsPageAnimated:YES
                                           completion:completion];
    return;
  }

  if (completion) {
    completion();
  }
}

- (void)animateSelectionUIExitWithCompletion:(void (^)())completion {
  __weak __typeof(self) weakSelf = self;
  __weak LensOverlayContainerPresenter* weakContainerPresenter =
      _containerPresenter;

  void (^onSelectionExitPositionSettled)() = ^{
    [weakSelf exitFullscreenAnimated:YES];
    if (!weakContainerPresenter) {
      if (completion) {
        completion();
      }

      return;
    }
    [weakContainerPresenter fadeSelectionUIWithCompletion:completion];
  };

  if (self.shouldResetSelectionToInitialPositionOnExit) {
    [_selectionViewController
        resetSelectionAreaToInitialPosition:onSelectionExitPositionSettled];
  } else {
    onSelectionExitPositionSettled();
  }
}

- (void)dismissLensOverlayWithCompletion:(void (^)())completion {
  [_containerPresenter dismissContainerAnimated:NO completion:completion];
}

#pragma mark - LensOverlayNetworkIssueDelegate

- (void)onNetworkIssueAlertWillShow {
  // Only one view controller may be presented at a time, so dismiss the bottom
  // sheet.
  [self stopResultPage];
}

- (void)onNetworkIssueAlertAcknowledged {
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kNetworkIssue];
}

#pragma mark - LensOverlayResultsPagePresenterDelegate

- (void)onResultsPageWillInitiateGestureDrivenDismiss {
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kBottomSheetDismissed];
}

- (void)onResultsPageDimensionStateChanged:(SheetDimensionState)state {
  if (_associatedTabHelper) {
    _associatedTabHelper->RecordSheetDimensionState(state);
  }

  switch (state) {
    case SheetDimensionStateHidden:
      [self destroyLensUI:YES
                   reason:lens::LensOverlayDismissalSource::
                              kBottomSheetDismissed];
      break;
    case SheetDimensionStateLarge:
      [self disableSelectionInteraction:YES];
      break;
    case SheetDimensionStateConsent:
      break;
    default:
      [self disableSelectionInteraction:NO];
      [_mediator defocusOmnibox];
      break;
  }
}

- (void)onResultsPageVerticalOcclusionInsetsSettled:(CGFloat)offsetNeeded {
  [_selectionViewController
      setOcclusionInsets:UIEdgeInsetsMake(0, 0, offsetNeeded, 0)
              reposition:YES
                animated:YES];
}

#pragma mark - LensOverlayMediatorDelegate

- (void)lensOverlayMediatorDidOpenOverlayMenu:(LensOverlayMediator*)mediator {
  // Capture the viewport snapshot before potential
  // navigation (e.g., user taps the "Learn More" button) to preserve the
  // current state.
  if (_associatedTabHelper) {
    _associatedTabHelper->RecordViewportSnaphot();
  }
  [_metricsRecorder recordOverflowMenuOpened];
}

- (void)lensOverlayMediatorOpenURLInNewTabRequsted:(GURL)URL {
  // Take a snapshot of the current tab before opening the URL in a new tab.
  // A side effect of opening a new tab is that the snapshot storage associated
  // to the current web state is updated. This snapshot would not include the
  // bottom sheet in the view hierarchy. Refrain from commiting it to
  // the storage until the web state is marked hidden, as by that point all
  // other updates should be issued.
  if (_associatedTabHelper) {
    _associatedTabHelper->RecordViewportSnaphot();
    _associatedTabHelper->RecordSheetDimensionState(
        _resultsPagePresenter.sheetDimension);
  }
  if (IsLensOverlaySameTabNavigationEnabled()) {
    CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
    [HandlerForProtocol(dispatcher, BrowserCoordinatorCommands)
        animateLensOverlayNavigationToURL:URL];
  } else {
    [self openURLInNewTab:URL];
    [self showRestorationWindowIfNeeded];
  }
}

- (void)respondToTabWillChange {
  if (!_associatedTabHelper) {
    return;
  }

  _associatedTabHelper->RecordViewportSnaphot();
  _associatedTabHelper->RecordSheetDimensionState(
      _resultsPagePresenter.sheetDimension);
  _associatedTabHelper->UpdateSnapshotStorage();
}

#pragma mark - LensOverlayResultConsumer

// This coordinator acts as a proxy consumer to the result consumer to implement
// lazy initialization of the result UI.
- (void)loadResultsURL:(GURL)url {
  [_metricsRecorder
      recordResultLoadedWithTextSelection:_mediator.currentLensResult
                                              .isTextSelection];

  if (!_resultMediator) {
    [self startResultPage];
  }

  [_resultMediator loadResultsURL:url];
}

- (void)handleSearchRequestStarted {
  [_resultMediator handleSearchRequestStarted];
}

- (void)handleSearchRequestErrored {
  if (_resultMediator) {
    [_resultMediator handleSearchRequestErrored];
  } else {
    [_networkIssueAlertPresenter showNoInternetAlert];
  }
}

- (void)handleSlowRequestHasStarted {
  if (!_resultMediator) {
    [self startResultPage];
  }
  [_resultMediator handleSlowRequestHasStarted];
}

#pragma mark - LensOverlayConsentViewControllerDelegate

- (void)didTapPrimaryActionButton {
  self.browser->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kLensOverlayConditionsAccepted, true);
  _consentViewController = nil;
  [_metricsRecorder recordPermissionsAccepted];

  __weak __typeof(self) weakSelf = self;
  [_lensOverlayConsentPresenter
      dismissConsentViewControllerAnimated:YES
                                completion:^{
                                  [weakSelf
                                      handleConsentViewControllerDismissed];
                                }];
}

- (void)didTapSecondaryActionButton {
  [_metricsRecorder recordPermissionsDenied];
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kLensPermissionsDenied];
}

- (void)didPressLearnMore {
  [_metricsRecorder recordPermissionsLinkOpen];
  [self openURLInNewTab:GURL(kLearnMoreLensURL)];
}

- (void)didRequestSearchWithCamera {
  [_metricsRecorder recordSearchWithCameraTapped];
  __weak __typeof(self) weakSelf = self;
  __weak id<LensCommands> weakLensHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(), LensCommands);
  [self
      hideLensUI:YES
      completion:^{
        OpenLensInputSelectionCommand* command =
            [[OpenLensInputSelectionCommand alloc]
                    initWithEntryPoint:LensEntrypoint::LensOverlayLvfEscapeHatch
                     presentationStyle:LensInputSelectionPresentationStyle::
                                           SlideFromRight
                presentationCompletion:^{
                  [weakSelf destroyLensUI:NO
                                   reason:lens::LensOverlayDismissalSource::
                                              kSearchWithCameraRequested];
                }];

        [weakLensHandler openLensInputSelection:command];
      }];
}

- (void)scheduleTooltipHintDisplay {
  if (_isExiting || _isStopped) {
    return;
  }
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf onTooltipScheduledDisplayDelayElapsed];
      }),
      kSearchWithCameraTooltipHintDelay);
}

- (void)onTooltipScheduledDisplayDelayElapsed {
  if (_isExiting || _isStopped) {
    return;
  }

  BOOL hadInteraction = self.isResultsBottomSheetCreated;
  if (!hadInteraction) {
    if ([_selectionViewController
            respondsToSelector:@selector(requestShowOverflowMenuTooltip)]) {
      [_selectionViewController requestShowOverflowMenuTooltip];
    }
  }
}

#pragma mark - LensOverlayConsentPresenterDelegate

- (void)requestDismissalOfConsentDialog:
    (LensOverlayConsentPresenter*)presenter {
  [self destroyLensUI:YES
               reason:lens::LensOverlayDismissalSource::kBottomSheetDismissed];
}

#pragma mark - private

- (void)prepareOverlayWithEntrypoint:(LensOverlayEntrypoint)entrypoint {
  if (self.isUICreated) {
    // The UI is probably associated with the non-active tab. Destroy it with no
    // animation.
    [self destroyLensUI:NO
                 reason:lens::LensOverlayDismissalSource::kNewLensInvocation];
  }

  [[NSNotificationCenter defaultCenter]
      addObserver:self
         selector:@selector(lowMemoryWarningReceived)
             name:UIApplicationDidReceiveMemoryWarningNotification
           object:nil];

  _entrypoint = entrypoint;
  _associatedTabHelper = self.activeTabHelper->GetWeakPtr();

  _metricsRecorder = [[LensOverlayMetricsRecorder alloc]
      initWithEntrypoint:entrypoint
      associatedWebState:_associatedTabHelper->GetWebState()];

  // The instance that creates the Lens UI designates itself as the command
  // handler for the associated tab.
  _associatedTabHelper->SetLensOverlayCommandsHandler(self);
  _associatedTabHelper->SetLensOverlayUIAttachedAndAlive(true);
}

- (void)openURLInNewTab:(GURL)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:URL
                   inIncognito:self.browser->GetProfile()->IsOffTheRecord()];

  [HandlerForProtocol(self.browser->GetCommandDispatcher(), ApplicationCommands)
      openURLInNewTab:command];
}

- (BOOL)shouldShowConsentFlow {
  if (lens::IsLVFEntrypoint(_entrypoint) ||
      lens::IsImageContextMenuEntrypoint(_entrypoint)) {
    return NO;
  }

  BOOL forceShowConsent =
      base::FeatureList::IsEnabled(kLensOverlayForceShowOnboardingScreen);

  return !self.termsOfServiceAccepted || forceShowConsent;
}

- (BOOL)termsOfServiceAccepted {
  if (!self.browser || !self.browser->GetProfile() ||
      !self.browser->GetProfile()->GetPrefs()) {
    return NO;
  }

  return self.browser->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kLensOverlayConditionsAccepted);
}

- (void)checkTermsOfServiceIfNeeded {
  if (lens::IsLVFEntrypoint(_entrypoint) ||
      lens::IsImageContextMenuEntrypoint(_entrypoint)) {
    return;
  }

  CHECK(self.termsOfServiceAccepted);
}

- (void)startResultPage {
  Browser* browser = self.browser;
  ProfileIOS* profile = browser->GetProfile();

  web::WebState::CreateParams params = web::WebState::CreateParams(profile);
  web::WebStateDelegate* browserWebStateDelegate =
      WebStateDelegateBrowserAgent::FromBrowser(browser);
  _resultMediator = [[LensResultPageMediator alloc]
       initWithWebStateParams:params
      browserWebStateDelegate:browserWebStateDelegate
                 webStateList:browser->GetWebStateList()
                  isIncognito:profile->IsOffTheRecord()];
  _resultMediator.applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  _resultMediator.snackbarHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), SnackbarCommands);
  _resultMediator.errorHandler = _networkIssueAlertPresenter;
  _resultMediator.delegate = _mediator;
  _mediator.resultConsumer = _resultMediator;

  _resultViewController = [[LensResultPageViewController alloc] init];
  _resultViewController.mutator = _resultMediator;
  _resultViewController.toolbarMutator = _mediator;

  _resultContextMenuProvider = [[ContextMenuConfigurationProvider alloc]
         initWithBrowser:browser
      baseViewController:_resultViewController
            baseWebState:_resultMediator.webState
           isLensOverlay:YES];
  _resultContextMenuProvider.delegate = _resultMediator;

  _resultMediator.consumer = _resultViewController;
  _resultMediator.webViewContainer = _resultViewController.webViewContainer;
  _resultMediator.contextMenuProvider = _resultContextMenuProvider;

  [self buildResultsBottomSheetPresentation];
  [self showResultsBottomSheet];

  // TODO(crbug.com/355179986): Implement omnibox navigation with
  // omnibox_delegate.
  auto omniboxClient = std::make_unique<LensOmniboxClient>(
      profile, feature_engagement::TrackerFactory::GetForProfile(profile),
      /*web_provider=*/_resultMediator,
      /*omnibox_delegate=*/_mediator);
  _mediator.omniboxClient = omniboxClient.get();

  _omniboxCoordinator = [[OmniboxCoordinator alloc]
      initWithBaseViewController:nil
                         browser:browser
                   omniboxClient:std::move(omniboxClient)
                   isLensOverlay:YES];

  // TODO(crbug.com/355179721): Add omnibox focus delegate.
  _omniboxCoordinator.presenterDelegate = _resultViewController;
  _omniboxCoordinator.isSearchOnlyUI = YES;
  [_omniboxCoordinator start];

  [_omniboxCoordinator.managedViewController
      willMoveToParentViewController:_resultViewController];
  [_resultViewController
      addChildViewController:_omniboxCoordinator.managedViewController];
  [_resultViewController setEditView:_omniboxCoordinator.editView];
  [_omniboxCoordinator.managedViewController
      didMoveToParentViewController:_resultViewController];

  [_omniboxCoordinator updateOmniboxState];

  _mediator.omniboxCoordinator = _omniboxCoordinator;
  _mediator.toolbarConsumer = _resultViewController;
  _omniboxCoordinator.focusDelegate = _mediator;
}

- (void)exitFullscreenAnimated:(BOOL)animated {
  Browser* browser = self.browser;
  if (!browser) {
    return;
  }

  FullscreenController* fullscreenController =
      FullscreenController::FromBrowser(browser);

  if (animated) {
    fullscreenController->ExitFullscreen();
  } else {
    fullscreenController->ExitFullscreenWithoutAnimation();
  }
}

- (void)stopResultPage {
  [_resultContextMenuProvider stop];
  _resultContextMenuProvider = nil;
  // The results view controller is still internally retained for the duration
  // of the animation.
  [_resultsPagePresenter dismissResultsPageAnimated:YES completion:nil];
  _resultViewController = nil;
  [_resultMediator disconnect];
  _resultMediator = nil;
  _mediator.resultConsumer = self;
  _mediator.omniboxClient = nil;
  [_omniboxCoordinator stop];
  _omniboxCoordinator = nil;
}

- (BOOL)isUICreated {
  return _containerViewController != nil;
}

- (BOOL)isResultsBottomSheetCreated {
  return _resultViewController != nil;
}

// Disconnect and destroy all of the owned view controllers.
- (void)destroyViewControllersAndMediators {
  [self stopResultPage];
  _containerViewController = nil;
  [_mediator disconnect];
  _selectionViewController = nil;
  _mediator = nil;
  _consentViewController = nil;
  _isExiting = NO;
  _associatedTabHelper = nullptr;
  _metricsRecorder = nil;
  _containerPresenter = nil;
  _resultsPagePresenter = nil;
  _lensOverlayConsentPresenter = nil;
  _networkIssueAlertPresenter = nil;
}

// The tab helper for the active web state.
- (LensOverlayTabHelper*)activeTabHelper {
  if (!self.browser || !self.browser->GetWebStateList() ||
      !self.browser->GetWebStateList()->GetActiveWebState()) {
    return nullptr;
  }

  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  LensOverlayTabHelper* tabHelper =
      LensOverlayTabHelper::FromWebState(activeWebState);

  CHECK(tabHelper, kLensOverlayNotFatalUntil);

  return tabHelper;
}

// Sets up the necessary utilities for turning on fullscreen as well as
// capturing a snapshot of the base window.
- (BOOL)prepareSnapshotCapturingInfrastructure {
  Browser* browser = self.browser;
  if (!browser) {
    return NO;
  }

  web::WebState* activeWebState =
      browser->GetWebStateList()->GetActiveWebState();

  UIWindow* sceneWindow = browser->GetSceneState().window;
  if (!sceneWindow || !_associatedTabHelper || !activeWebState) {
    return NO;
  }

  _associatedTabHelper->SetSnapshotController(
      std::make_unique<LensOverlaySnapshotController>(
          SnapshotTabHelper::FromWebState(activeWebState),
          FullscreenController::FromBrowser(browser), sceneWindow,
          IsCurrentLayoutBottomOmnibox(browser)));

  return YES;
}

// Captures a screenshot of the active web state.
- (void)captureSnapshotWithCompletion:(void (^)(UIImage*))completion {
  BOOL success = [self prepareSnapshotCapturingInfrastructure];
  if (!success) {
    if (completion) {
      completion(nil);
    }

    return;
  }

  _associatedTabHelper->CaptureFullscreenSnapshot(base::BindOnce(completion));
}

- (void)lowMemoryWarningReceived {
  // Preserve the UI if it's currently visible to the user.
  if ([self isLensOverlayVisible]) {
    return;
  }

  [self destroyLensUI:NO reason:lens::LensOverlayDismissalSource::kLowMemory];
}

// Whether the image should be repositioned when exiting.
- (BOOL)shouldResetSelectionToInitialPositionOnExit {
  return _entrypoint != LensOverlayEntrypoint::kSearchImageContextMenu &&
         _entrypoint != LensOverlayEntrypoint::kLVFImagePicker;
}

- (BOOL)isLensOverlayVisible {
  return _containerPresenter.isLensOverlayVisible;
}

// Blocks user interaction with the Lens UI.
- (void)disableSelectionInteraction:(BOOL)disabled {
  _containerViewController.selectionInteractionDisabled = disabled;
  [_selectionViewController disableFlyoutMenu:disabled];
}

// Called after consent dialog was dismissed and TOS accepted.
- (void)handleConsentViewControllerDismissed {
  if (_isExiting || _isStopped) {
    return;
  }

  [self checkTermsOfServiceIfNeeded];
  [self disableSelectionInteraction:NO];
  [_selectionViewController setTopIconsHidden:NO];
  [_selectionViewController start];

  if (IsLVFEscapeHatchEnabled()) {
    [self scheduleTooltipHintDisplay];
  }
}

- (void)buildResultsBottomSheetPresentation {
  _resultsPagePresenter = [[LensOverlayResultsPagePresenter alloc]
      initWithBaseViewController:_containerViewController
        resultPageViewController:_resultViewController];

  _resultsPagePresenter.delegate = self;
  _resultMediator.presentationDelegate = _resultsPagePresenter;
  _mediator.presentationDelegate = _resultsPagePresenter;
}

- (void)showResultsBottomSheet {
  if (!_associatedTabHelper) {
    return;
  }

  __weak __typeof(self) weakSelf = self;

  SheetDimensionState restoredSheetState =
      _associatedTabHelper->GetRecordedSheetDimensionState();
  BOOL isStateRestoration = restoredSheetState != SheetDimensionStateHidden;
  BOOL maximizeSheet = restoredSheetState == SheetDimensionStateLarge;
  [_resultsPagePresenter
      presentResultsPageAnimated:!isStateRestoration
                   maximizeSheet:maximizeSheet
                startInTranslate:_selectionViewController.translateFilterActive
                      completion:^{
                        [weakSelf resultsBottomSheetPresented];
                      }];
}

- (void)showRestorationWindowIfNeeded {
  // If there is a pending snapshot, show it in a separate fullscreen window to
  // ease the transition.
  UIWindow* sceneWindow = self.browser->GetSceneState().window;
  if (!_associatedTabHelper || !sceneWindow) {
    return;
  }

  // The Lens overlay is locked to portrait. Skip displaying the restoration
  // window on landscape to avoid stretching the snapshot.
  if (IsLandscape(sceneWindow) && !IsLensOverlayLandscapeOrientationEnabled()) {
    return;
  }

  UIImage* viewportSnapshot = _associatedTabHelper->GetViewportSnapshot();
  // If no snapshot was stored, it means that a restoration of state is not
  // needed.
  if (!viewportSnapshot) {
    return;
  }
  _restorationWindow =
      [[UIWindow alloc] initWithWindowScene:sceneWindow.windowScene];
  _restorationWindow.rootViewController =
      [[SnapshotCoverViewController alloc] initWithImage:viewportSnapshot];
  _restorationWindow.windowLevel = sceneWindow.windowLevel + 1;
  _restorationWindow.hidden = NO;
}

- (void)dismissRestorationWindow {
  _restorationWindow.hidden = YES;
  _restorationWindow = nil;
}

- (void)resultsBottomSheetPresented {
  [self dismissRestorationWindow];
  if (_associatedTabHelper) {
    _associatedTabHelper->ClearViewportSnapshot();
  }
}

@end
