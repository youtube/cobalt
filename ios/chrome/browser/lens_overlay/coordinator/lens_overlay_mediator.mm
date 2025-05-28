// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator.h"

#import <memory>

#import "base/base64url.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/timer/elapsed_timer.h"
#import "components/lens/lens_overlay_metrics.h"
#import "components/lens/proto/server/lens_overlay_response.pb.h"
#import "components/search_engines/template_url_service.h"
#import "ios/chrome/browser/default_browser/model/default_browser_interest_signals.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_omnibox_client.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_mediator_delegate.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_manager.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_navigation_mutator.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"
#import "ios/chrome/browser/lens_overlay/public/lens_overlay_constants.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_toolbar_consumer.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_coordinator.h"
#import "ios/chrome/browser/orchestrator/ui_bundled/edit_view_animatee.h"
#import "ios/chrome/browser/search_engines/model/search_engine_observer_bridge.h"
#import "ios/chrome/browser/search_engines/model/search_engines_util.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/public/provider/chrome/browser/lens/lens_overlay_result.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

namespace {

// Different filter states for lens overlay.
typedef NS_ENUM(NSUInteger, LensOverlayFilterState) {
  LensOverlayFilterStateUnknown = 0,
  LensOverlayFilterStateSelection,
  LensOverlayFilterStateTranslate,
};

}  // namespace

@interface LensOverlayMediator () <LensOverlayNavigationMutator,
                                   SearchEngineObserving>

/// Current lens result.
@property(nonatomic, strong, readwrite) id<ChromeLensOverlayResult>
    currentLensResult;
/// Number of tab opened by the lens overlay.
@property(nonatomic, assign, readwrite) NSInteger generatedTabCount;

@end

@implementation LensOverlayMediator {
  /// Whether the browser is off the record.
  BOOL _isIncognito;
  /// Search engine observer.
  std::unique_ptr<SearchEngineObserverBridge> _searchEngineObserver;
  /// Orchestrates the navigation in the bottom sheet of the lens result page.
  std::unique_ptr<LensOverlayNavigationManager> _navigationManager;
  /// Time where lens started the search request.
  base::ElapsedTimer _lensStartSearchRequestTime;
  /// Whether the thumbnail/selection of the `currentLensResult` was removed.
  BOOL _thumbnailRemoved;
  /// Tracks the Lens filter currently in use.
  LensOverlayFilterState _currentFilterState;
}

- (instancetype)initWithIsIncognito:(BOOL)isIncognito {
  self = [super init];
  if (self) {
    _isIncognito = isIncognito;
    _navigationManager = std::make_unique<LensOverlayNavigationManager>(self);
  }
  return self;
}

- (void)setTemplateURLService:(TemplateURLService*)templateURLService {
  _templateURLService = templateURLService;
  if (_templateURLService) {
    _searchEngineObserver =
        std::make_unique<SearchEngineObserverBridge>(self, _templateURLService);
    [self searchEngineChanged];
  } else {
    _searchEngineObserver.reset();
  }
}

- (void)disconnect {
  _searchEngineObserver.reset();
  _navigationManager.reset();
  _currentLensResult = nil;
  _currentFilterState = LensOverlayFilterStateUnknown;
}

#pragma mark - SearchEngineObserving

- (void)searchEngineChanged {
  BOOL isLensAvailable =
      search_engines::SupportsSearchImageWithLens(_templateURLService);
  if (!isLensAvailable) {
    [self.commandsHandler destroyLensUI:YES
                                 reason:lens::LensOverlayDismissalSource::
                                            kDefaultSearchEngineChange];
  }
}

#pragma mark - Omnibox

#pragma mark LensOmniboxClientDelegate

- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
               textClobbered:(BOOL)textClobbered {
  [self defocusOmnibox];

  const BOOL isUnimodalTextQuery =
      _thumbnailRemoved || _currentLensResult.isTextSelection;
  if (isUnimodalTextQuery) {
    if (textClobbered) {
      if (IsLensOverlaySameTabNavigationEnabled()) {
        __weak LensOverlayMediator* weakSelf = self;
        // Delay navigation until after omnibox defocus and toolbar button hide
        // animations complete. This ensures a smooth transition and avoids
        // interrupting the UI animations.
        GURL URL = destinationURL;
        CGFloat totalAnimationDuration =
            kLensResultPageButtonAnimationDuration +
            kLensResultPageToolbarLayoutDuration;
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                     totalAnimationDuration * NSEC_PER_SEC),
                       dispatch_get_main_queue(), ^{
                         [weakSelf.delegate
                             lensOverlayMediatorOpenURLInNewTabRequsted:URL];
                       });
      } else {
        [self.delegate
            lensOverlayMediatorOpenURLInNewTabRequsted:destinationURL];
      }

      [self recordNewTabGeneratedBy:lens::LensOverlayNewTabSource::kOmnibox];
      if (_omniboxClient) {
        [self updateOmniboxText:_omniboxClient->GetOmniboxSteadyStateText()];
      }
    } else if (_navigationManager) {
      // Hide the Lens selection as the omnibox content no longer reflect it.
      [self.lensHandler hideUserSelection];
      _navigationManager->LoadUnimodalOmniboxNavigation(destinationURL, text);
    }
  } else {  // Multimodal query.
    // Setting the query text generates new results.
    NSString* nsText = [NSString cr_fromString16:text];
    [self updateOmniboxText:nsText];
    [self.lensHandler setQueryText:nsText clearSelection:_thumbnailRemoved];
  }
}

- (void)omniboxDidRemoveThumbnail {
  _thumbnailRemoved = YES;
  [self.lensHandler hideUserSelection];
}

#pragma mark LensToolbarMutator

- (void)focusOmnibox {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.FocusOmnibox"));
  [self.omniboxCoordinator focusOmnibox];
  [self.toolbarConsumer setOmniboxFocused:YES];
  [self.omniboxCoordinator.animatee setClearButtonFaded:NO];
  [self.presentationDelegate requestMaximizeBottomSheet];
}

- (void)defocusOmnibox {
  [self.omniboxCoordinator endEditing];
  [self.toolbarConsumer setOmniboxFocused:NO];
  [self.omniboxCoordinator.animatee setClearButtonFaded:YES];
}

- (void)goBack {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.Back"));
  if (_navigationManager) {
    _navigationManager->GoBack();
  }
}

#pragma mark OmniboxFocusDelegate

- (void)omniboxDidBecomeFirstResponder {
  [self focusOmnibox];
}

- (void)omniboxDidResignFirstResponder {
  [self defocusOmnibox];
}

#pragma mark - ChromeLensOverlayDelegate

// The lens overlay started searching for a result.
- (void)lensOverlayDidStartSearchRequest:(id<ChromeLensOverlay>)lensOverlay {
  [self.resultConsumer handleSearchRequestStarted];
  _lensStartSearchRequestTime = base::ElapsedTimer();
  [self.toolbarConsumer setOmniboxEnabled:YES];

  // If the filter is still unknown it means this is the first request, so
  // nothing needs to be done, as the selection area in the zero state is
  // correctly positioned.
  if (_currentFilterState != LensOverlayFilterStateUnknown) {
    BOOL isInTranslate = _currentFilterState == LensOverlayFilterStateTranslate;
    BOOL willUseTranslate = self.lensHandler.translateFilterActive;

    BOOL switchToTranslate = !isInTranslate && willUseTranslate;
    BOOL switchToSelection = isInTranslate && !willUseTranslate;

    if (switchToTranslate) {
      // The translation filter needs the selection area reset as well as the
      // bottom sheet hidden, as no auto selection happens at this stage.
      [self.lensHandler resetSelectionAreaToInitialPosition:^{
      }];
      [self.presentationDelegate hideBottomSheet];
    } else if (switchToSelection || willUseTranslate) {
      // When transitioning to selection the bottom sheet might be hidden. As
      // auto selection might be on we need to restore it if hidden.
      [self.presentationDelegate revealBottomSheetIfHidden];
    }
  }

  _currentFilterState = self.lensHandler.translateFilterActive
                            ? LensOverlayFilterStateTranslate
                            : LensOverlayFilterStateSelection;
}

// The lens overlay search request produced an error.
- (void)lensOverlayDidReceiveError:(id<ChromeLensOverlay>)lensOverlay {
  [self.resultConsumer handleSearchRequestErrored];
  [self.toolbarConsumer setOmniboxEnabled:YES];
}

// The lens overlay search request produced a valid result.
- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didGenerateResult:(id<ChromeLensOverlayResult>)result {
  RecordAction(base::UserMetricsAction("Mobile.LensOverlay.NewResult"));
  lens::RecordLensResponseTime(_lensStartSearchRequestTime.Elapsed());
  if (_navigationManager) {
    _navigationManager->LensOverlayDidGenerateResult(result);
  }
  [self.toolbarConsumer setOmniboxEnabled:YES];
}

- (void)lensOverlayDidTapOnCloseButton:(id<ChromeLensOverlay>)lensOverlay {
  [self.commandsHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kOverlayCloseButton];
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    hasSuggestSignalsAvailableOnResult:(id<ChromeLensOverlayResult>)result {
  if (result != _currentLensResult) {
    return;
  }
  [self setOmniboxSuggestSignals:result];
}

- (void)lensOverlay:(id<ChromeLensOverlay>)lensOverlay
    didRequestToOpenURL:(GURL)URL {
  [self.resultConsumer loadResultsURL:URL];
}

- (void)lensOverlayDidOpenOverlayMenu:(id<ChromeLensOverlay>)lensOverlay {
  [self.delegate lensOverlayMediatorDidOpenOverlayMenu:self];
}

- (void)lensOverlayDidDeferGesture:(id<ChromeLensOverlay>)lensOverlay {
  [self.resultConsumer handleSlowRequestHasStarted];
  UIImage* placeholder = ImageWithColor([UIColor colorNamed:kGrey200Color]);
  [self.omniboxCoordinator setThumbnailImage:placeholder];
  [self.toolbarConsumer setOmniboxEnabled:NO];
}

#pragma mark - LensOverlayNavigationMutator

- (void)loadLensResult:(id<ChromeLensOverlayResult>)result {
  _currentLensResult = result;
  _thumbnailRemoved = NO;
  // Load the URL, it will start the result UI.
  [self.resultConsumer loadResultsURL:result.searchResultURL];
  [self updateForLensResult:result];
}

- (void)reloadLensResult:(id<ChromeLensOverlayResult>)result {
  // Pre update the UI.
  [self updateForLensResult:result];
  // Reload the result.
  [self.lensHandler reloadResult:result];
}

- (void)loadURL:(const GURL&)URL omniboxText:(NSString*)omniboxText {
  // Restore the thumbnail when navigating back to an LRP.
  if (!_currentLensResult.isTextSelection && _thumbnailRemoved &&
      !lens::IsLensOverlaySRP(URL)) {
    _thumbnailRemoved = NO;
    [self.omniboxCoordinator
        setThumbnailImage:_currentLensResult.selectionPreviewImage];
  }
  [self updateOmniboxText:omniboxText];
  [self.resultConsumer loadResultsURL:URL];
}

- (void)onBackNavigationAvailabilityMaybeChanged:(BOOL)canGoBack {
  [self.toolbarConsumer setCanGoBack:canGoBack];
}

- (void)onSRPLoadWithOmniboxText:(NSString*)omniboxText {
  if (![omniboxText isEqualToString:_currentLensResult.queryText]) {
    if (!_currentLensResult.isTextSelection) {
      [self.omniboxCoordinator setThumbnailImage:nil];
      _thumbnailRemoved = YES;
    }
    [self.lensHandler hideUserSelection];
  }
  [self updateOmniboxText:omniboxText];
}

#pragma mark - LensResultPageMediatorDelegate

- (void)lensResultPageWebStateDestroyed {
  [self.commandsHandler
      destroyLensUI:YES
             reason:lens::LensOverlayDismissalSource::kTabClosed];
}

- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState {
  if (_navigationManager) {
    _navigationManager->SetWebState(webState);
  }
}

- (void)lensResultPageMediator:(LensResultPageMediator*)mediator
       didOpenNewTabFromSource:(lens::LensOverlayNewTabSource)newTabSource {
  [self recordNewTabGeneratedBy:newTabSource];
}

- (void)lensResultPageOpenURLInNewTabRequsted:(GURL)URL {
  [self.delegate lensOverlayMediatorOpenURLInNewTabRequsted:URL];
}

- (void)respondToTabWillChange {
  [self.delegate respondToTabWillChange];
}

#pragma mark - Private

/// Updates the UI for lens `result`.
- (void)updateForLensResult:(id<ChromeLensOverlayResult>)result {
  [self.omniboxCoordinator
      setThumbnailImage:result.isTextSelection ? nil
                                               : result.selectionPreviewImage];
  if (self.omniboxClient) {
    [self setOmniboxSuggestSignals:result];
    self.omniboxClient->SetLensResultHasThumbnail(!result.isTextSelection);
  }
  [self updateOmniboxText:result.queryText];

  if (result.isGeneratedInTranslate) {
    [self.presentationDelegate didLoadTranslateResult];
  } else {
    [self.presentationDelegate didLoadSelectionResult];
  }
}

/// Updates the steady state omnibox text.
- (void)updateOmniboxText:(NSString*)text {
  if (self.omniboxClient) {
    self.omniboxClient->SetOmniboxSteadyStateText(text);
  }
  [self.omniboxCoordinator updateOmniboxState];
}

/// Sets the omnibox suggest signals with `result`.
- (void)setOmniboxSuggestSignals:(id<ChromeLensOverlayResult>)result {
  if (!self.omniboxClient) {
    return;
  }

  NSData* data = result.suggestSignals;
  if (!data.length) {
    self.omniboxClient->SetLensOverlaySuggestInputs(std::nullopt);
    return;
  }
  std::string encodedString;
  base::span<const uint8_t> signals = base::span<const uint8_t>(
      static_cast<const uint8_t*>(result.suggestSignals.bytes),
      result.suggestSignals.length);

  Base64UrlEncode(signals, base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                  &encodedString);

  if (!encodedString.empty()) {
    lens::proto::LensOverlaySuggestInputs response;
    response.set_encoded_image_signals(encodedString);
    self.omniboxClient->SetLensOverlaySuggestInputs(response);
  }
}

/// Records lens overlay opening a new tab.
- (void)recordNewTabGeneratedBy:(lens::LensOverlayNewTabSource)newTabSource {
  self.generatedTabCount += 1;
  lens::RecordNewTabGenerated(newTabSource);
}

@end
