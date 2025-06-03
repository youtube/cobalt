// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/promos_manager/promos_manager_coordinator.h"

#import <Foundation/Foundation.h>
#import <map>

#import "base/check.h"
#import "base/containers/small_map.h"
#import "base/debug/dump_without_crashing.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/crash/core/common/crash_key.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/default_browser/model/utils.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/ntp/features.h"
#import "ios/chrome/browser/promos_manager/features.h"
#import "ios/chrome/browser/promos_manager/promo_config.h"
#import "ios/chrome/browser/promos_manager/promos_manager.h"
#import "ios/chrome/browser/promos_manager/promos_manager_factory.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/ui/app_store_rating/app_store_rating_display_handler.h"
#import "ios/chrome/browser/ui/app_store_rating/features.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_display_handler.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/features.h"
#import "ios/chrome/browser/ui/default_promo/post_restore/post_restore_default_browser_promo_provider.h"
#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_promo_display_handler.h"
#import "ios/chrome/browser/ui/default_promo/promo_handler/default_browser_remind_me_later_promo_display_handler.h"
#import "ios/chrome/browser/ui/post_restore_signin/post_restore_signin_provider.h"
#import "ios/chrome/browser/ui/promos_manager/bannered_promo_view_provider.h"
#import "ios/chrome/browser/ui/promos_manager/promos_manager_mediator.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_alert_provider.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_display_handler.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_view_provider.h"
#import "ios/chrome/browser/ui/whats_new/promo/whats_new_promo_display_handler.h"
#import "ios/chrome/browser/ui/whats_new/whats_new_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"
#import "ios/chrome/common/ui/promo_style/promo_style_view_controller_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/choice_api.h"
#import "third_party/abseil-cpp/absl/types/optional.h"
#import "ui/base/l10n/l10n_util_mac.h"

@interface PromosManagerCoordinator () <
    ConfirmationAlertActionHandler,
    UIAdaptivePresentationControllerDelegate,
    PromoStyleViewControllerDelegate> {
  // Promos that conform to the StandardPromoDisplayHandler protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoDisplayHandler>>>
      _displayHandlerPromos;

  // Promos that conform to the StandardPromoViewProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoViewProvider>>>
      _viewProviderPromos;

  // Promos that conform to the BanneredPromoViewProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<BanneredPromoViewProvider>>>
      _banneredViewProviderPromos;

  // Promos that conform to the StandardPromoAlertProvider protocol.
  base::small_map<
      std::map<promos_manager::Promo, id<StandardPromoAlertProvider>>>
      _alertProviderPromos;

  // The currently displayed promo data, if any.
  absl::optional<PromoDisplayData> _currentPromoData;

  // The handler for the CredentialProviderPromoCommands.
  id<CredentialProviderPromoCommands> _credentialProviderPromoCommandHandler;
}

// A mediator that observes when it's a good time to display a promo.
@property(nonatomic, strong) PromosManagerMediator* mediator;

// The current StandardPromoViewProvider, if any.
@property(nonatomic, weak) id<StandardPromoViewProvider> provider;

// The current BanneredPromoViewProvider, if any.
@property(nonatomic, weak) id<BanneredPromoViewProvider> banneredProvider;

// The current ConfirmationAlertViewController, if any.
@property(nonatomic, strong) ConfirmationAlertViewController* viewController;

// The current PromoStyleViewController, if any.
@property(nonatomic, strong) PromoStyleViewController* banneredViewController;

@end

@implementation PromosManagerCoordinator

#pragma mark - Initialization

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
            credentialProviderPromoHandler:
                (id<CredentialProviderPromoCommands>)handler {
  if (self = [super initWithBaseViewController:viewController
                                       browser:browser]) {
    _credentialProviderPromoCommandHandler = handler;
    [self registerPromos];

    BOOL promosExist = _displayHandlerPromos.size() > 0 ||
                       _viewProviderPromos.size() > 0 ||
                       _banneredViewProviderPromos.size() > 0 ||
                       _alertProviderPromos.size() > 0;

    if (promosExist) {
      // Don't create PromosManagerMediator unless promos exist that are
      // registered with PromosManagerCoordinator via `registerPromos`.
      PromosManager* promosManager =
          PromosManagerFactory::GetForBrowserState(browser->GetBrowserState());
      _mediator = [[PromosManagerMediator alloc]
          initWithPromosManager:promosManager
          promoImpressionLimits:[self promoImpressionLimits]];
    }
  }

  return self;
}

#pragma mark - Public

- (void)start {
  [self displayPromoIfAvailable:YES];
}

- (void)stop {
  self.mediator = nil;
  [self dismissViewControllers];
}

- (void)displayPromoIfAvailable {
  [self displayPromoIfAvailable:NO];
}

// Display a promo if one is available, with special behavior if this is the
// first time this coordinator has shown a promo.
- (void)displayPromoIfAvailable:(BOOL)isFirstShownPromo {
  if (ShouldPromosManagerUseFET()) {
    // Wait to present a promo until the feature engagement tracker database
    // is fully initialized.
    __weak __typeof(self) weakSelf = self;
    void (^onInitializedBlock)(bool) = ^(bool successfullyLoaded) {
      if (!successfullyLoaded) {
        return;
      }
      [weakSelf displayPromoCallback:isFirstShownPromo];
    };

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    tracker->AddOnInitializedCallback(base::BindOnce(onInitializedBlock));
  } else {
    [self displayPromoCallback:isFirstShownPromo];
  }
}

- (void)displayPromoCallback:(BOOL)isFirstShownPromo {
  // If there's already a displayed promo, skip.
  if (_currentPromoData.has_value()) {
    return;
  }

  absl::optional<PromoDisplayData> nextPromoForDisplay =
      [self.mediator nextPromoForDisplay:isFirstShownPromo];

  if (nextPromoForDisplay.has_value()) {
    [self displayPromo:nextPromoForDisplay.value()];
  }
}

- (void)dismissViewControllers {
  if (self.viewController) {
    [self.viewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    self.viewController = nil;
  }

  if (self.banneredViewController) {
    [self.banneredViewController.presentingViewController
        dismissViewControllerAnimated:YES
                           completion:nil];
    self.banneredViewController = nil;
  }

  [self promoWasDismissed];
}

- (void)promoWasDismissed {
  if (ShouldPromosManagerUseFET() && _currentPromoData.has_value() &&
      !_currentPromoData.value().was_forced) {
    PromoConfigsSet configs = [self promoImpressionLimits];
    auto it = configs.find(_currentPromoData.value().promo);
    if (it == configs.end() || !it->feature_engagement_feature) {
      return;
    }

    feature_engagement::Tracker* tracker =
        feature_engagement::TrackerFactory::GetForBrowserState(
            self.browser->GetBrowserState());
    tracker->Dismissed(*it->feature_engagement_feature);
  }
  _currentPromoData = absl::nullopt;
}

- (void)displayPromo:(PromoDisplayData)promoData {
  if (tests_hook::DisablePromoManagerFullScreenPromos()) {
    return;
  }

  promos_manager::Promo promo = promoData.promo;
  _currentPromoData = promoData;

  auto handler_it = _displayHandlerPromos.find(promo);
  auto provider_it = _viewProviderPromos.find(promo);
  auto bannered_provider_it = _banneredViewProviderPromos.find(promo);
  auto alert_provider_it = _alertProviderPromos.find(promo);

  id<PromosManagerCommands> promosManagerCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), PromosManagerCommands);

  if (handler_it != _displayHandlerPromos.end()) {
    id<StandardPromoDisplayHandler> handler = handler_it->second;

    if ([handler respondsToSelector:@selector(setHandler:)])
      handler.handler = promosManagerCommandsHandler;

    [handler handleDisplay];

    [self.mediator recordImpression:handler.config.identifier];

    base::UmaHistogramEnumeration("IOS.PromosManager.Promo", promo);
    base::UmaHistogramEnumeration("IOS.PromosManager.Promo.Type",
                                  promos_manager::IOSPromosManagerPromoType::
                                      kStandardPromoDisplayHandler);

    if ([handler respondsToSelector:@selector(promoWasDisplayed)]) {
      [handler promoWasDisplayed];
    }
  } else if (provider_it != _viewProviderPromos.end()) {
    id<StandardPromoViewProvider> provider = provider_it->second;

    if ([provider respondsToSelector:@selector(setHandler:)])
      provider.handler = promosManagerCommandsHandler;

    self.viewController = [provider viewController];
    self.viewController.presentationController.delegate = self;
    self.viewController.actionHandler = self;

    self.provider = provider;

    [self.baseViewController presentViewController:self.viewController
                                          animated:YES
                                        completion:nil];

    [self.mediator recordImpression:provider.config.identifier];

    base::UmaHistogramEnumeration("IOS.PromosManager.Promo", promo);
    base::UmaHistogramEnumeration(
        "IOS.PromosManager.Promo.Type",
        promos_manager::IOSPromosManagerPromoType::kStandardPromoViewProvider);

    if ([provider respondsToSelector:@selector(promoWasDisplayed)]) {
      [provider promoWasDisplayed];
    }
  } else if (bannered_provider_it != _banneredViewProviderPromos.end()) {
    id<BanneredPromoViewProvider> banneredProvider =
        bannered_provider_it->second;

    if ([banneredProvider respondsToSelector:@selector(setHandler:)])
      banneredProvider.handler = promosManagerCommandsHandler;

    self.banneredViewController = [banneredProvider viewController];

    self.banneredViewController.presentationController.delegate = self;
    self.banneredViewController.delegate = self;
    self.banneredProvider = banneredProvider;

    [self.baseViewController presentViewController:self.banneredViewController
                                          animated:YES
                                        completion:nil];

    [self.mediator recordImpression:banneredProvider.config.identifier];

    base::UmaHistogramEnumeration("IOS.PromosManager.Promo", promo);
    base::UmaHistogramEnumeration(
        "IOS.PromosManager.Promo.Type",
        promos_manager::IOSPromosManagerPromoType::kBanneredPromoViewProvider);

    if ([banneredProvider respondsToSelector:@selector(promoWasDisplayed)]) {
      [banneredProvider promoWasDisplayed];
    }
  } else if (alert_provider_it != _alertProviderPromos.end()) {
    id<StandardPromoAlertProvider> alertProvider = alert_provider_it->second;

    if ([alertProvider respondsToSelector:@selector(setHandler:)])
      alertProvider.handler = promosManagerCommandsHandler;

    DCHECK([alertProvider.title length] != 0);
    DCHECK([alertProvider.message length] != 0);
    // The "Default Action" should always be implemented by feature
    DCHECK([alertProvider
        respondsToSelector:@selector(standardPromoAlertDefaultAction)]);

    UIAlertController* alert = [UIAlertController
        alertControllerWithTitle:alertProvider.title
                         message:alertProvider.message
                  preferredStyle:UIAlertControllerStyleAlert];

    NSString* defaultActionButtonText =
        [alertProvider respondsToSelector:@selector(defaultActionButtonText)]
            ? alertProvider.defaultActionButtonText
            : l10n_util::GetNSString(
                  IDS_IOS_PROMOS_MANAGER_ALERT_PROMO_DEFAULT_PRIMARY_BUTTON_TEXT);
    NSString* cancelActionButtonText =
        [alertProvider respondsToSelector:@selector(cancelActionButtonText)]
            ? alertProvider.cancelActionButtonText
            : l10n_util::GetNSString(
                  IDS_IOS_PROMOS_MANAGER_ALERT_PROMO_DEFAULT_CANCEL_BUTTON_TEXT);

    UIAlertAction* defaultAction = [UIAlertAction
        actionWithTitle:defaultActionButtonText
                  style:UIAlertActionStyleDefault
                handler:^(UIAlertAction* action) {
                  if ([alertProvider respondsToSelector:@selector
                                     (standardPromoAlertDefaultAction)])
                    [alertProvider standardPromoAlertDefaultAction];

                  [self dismissViewControllers];
                }];

    UIAlertAction* cancelAction = [UIAlertAction
        actionWithTitle:cancelActionButtonText
                  style:UIAlertActionStyleCancel
                handler:^(UIAlertAction* action) {
                  if ([alertProvider respondsToSelector:@selector
                                     (standardPromoAlertCancelAction)]) {
                    [alertProvider standardPromoAlertCancelAction];
                  }
                  [self dismissViewControllers];
                }];

    [alert addAction:defaultAction];
    [alert addAction:cancelAction];
    alert.preferredAction = defaultAction;

    [self.baseViewController presentViewController:alert
                                          animated:YES
                                        completion:nil];

    [self.mediator recordImpression:alertProvider.config.identifier];

    base::UmaHistogramEnumeration("IOS.PromosManager.Promo", promo);
    base::UmaHistogramEnumeration(
        "IOS.PromosManager.Promo.Type",
        promos_manager::IOSPromosManagerPromoType::kStandardPromoAlertProvider);

    if ([alertProvider respondsToSelector:@selector(promoWasDisplayed)]) {
      [alertProvider promoWasDisplayed];
    }
  } else {
    // Deregister the promo in edge cases:
    //
    // 1. When promos are forced for display (via Experimental Settings toggle)
    // but not properly enabled (via chrome://flags).
    //
    // 2. When the promo's flag is disabled but was registered before and hasn't
    // been displayed yet.
    //
    // These are niche edge cases that almost exclusively occur during local,
    // manual testing.
    absl::optional<promos_manager::Promo> maybeForcedPromo =
        promos_manager::PromoForName(base::SysNSStringToUTF8(
            experimental_flags::GetForcedPromoToDisplay()));

    if (maybeForcedPromo.has_value()) {
      promos_manager::Promo forcedPromo = maybeForcedPromo.value();

      if ([self isPromoUnregistered:forcedPromo]) {
        base::UmaHistogramEnumeration(
            "IOS.PromosManager.Promo.ForcedDisplayFailure", forcedPromo);
      }
    } else {
      base::UmaHistogramEnumeration("IOS.PromosManager.Promo.DisplayFailure",
                                    promo);

      [self.mediator deregisterPromo:promo];
    }
  }
}

#pragma mark - PromoStyleViewControllerDelegate

// Invoked when the primary action button is tapped.
- (void)didTapPrimaryActionButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoPrimaryAction)])
    return;

  [self.banneredProvider standardPromoPrimaryAction];
}

// Invoked when the secondary action button is tapped.
- (void)didTapSecondaryActionButton {
  DCHECK(self.banneredProvider);

  // Sometimes the secondary action button for a PromoStyleViewController is
  // used as the dismiss action button.
  if ([self.banneredProvider
          respondsToSelector:@selector(standardPromoDismissAction)]) {
    [self.banneredProvider standardPromoDismissAction];
    [self dismissViewControllers];
  } else if ([self.banneredProvider
                 respondsToSelector:@selector(standardPromoSecondaryAction)]) {
    [self.banneredProvider standardPromoSecondaryAction];
  }
}

// Invoked when the tertiary action button is tapped.
- (void)didTapTertiaryActionButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoTertiaryAction)])
    return;

  [self.banneredProvider standardPromoTertiaryAction];
}

// Invoked when the top left question mark button is tapped.
- (void)didTapLearnMoreButton {
  DCHECK(self.banneredProvider);

  if (![self.banneredProvider
          respondsToSelector:@selector(standardPromoLearnMoreAction)])
    return;

  [self.banneredProvider standardPromoLearnMoreAction];
}

// Invoked when a link in the disclaimer is tapped.
- (void)didTapURLInDisclaimer:(NSURL*)URL {
  // TODO(crbug.com/1363906): Complete `didTapURLInDisclaimer` to bring users to
  // Settings page.
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  DCHECK(self.provider);

  if (![self.provider respondsToSelector:@selector(standardPromoPrimaryAction)])
    return;

  [self.provider standardPromoPrimaryAction];
}

- (void)confirmationAlertSecondaryAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoSecondaryAction)])
    return;

  [self.provider standardPromoSecondaryAction];
}

- (void)confirmationAlertTertiaryAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoTertiaryAction)])
    return;

  [self.provider standardPromoTertiaryAction];
}

- (void)confirmationAlertLearnMoreAction {
  DCHECK(self.provider);

  if (![self.provider
          respondsToSelector:@selector(standardPromoLearnMoreAction)])
    return;

  [self.provider standardPromoLearnMoreAction];
}

- (void)confirmationAlertDismissAction {
  DCHECK(self.provider || self.banneredProvider);

  if ([self.provider
          respondsToSelector:@selector(standardPromoDismissAction)]) {
    [self.provider standardPromoDismissAction];
    [self dismissViewControllers];
  } else if ([self.banneredProvider
                 respondsToSelector:@selector(standardPromoDismissAction)]) {
    [self.banneredProvider standardPromoDismissAction];
    [self dismissViewControllers];
  }
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  DCHECK(self.provider || self.banneredProvider);

  if ([self.provider respondsToSelector:@selector(standardPromoDismissSwipe)]) {
    [self.provider standardPromoDismissSwipe];
    [self dismissViewControllers];
  } else if ([self.banneredProvider
                 respondsToSelector:@selector(standardPromoDismissSwipe)]) {
    [self.banneredProvider standardPromoDismissSwipe];
    [self dismissViewControllers];
  } else {
    [self confirmationAlertDismissAction];
  }
}

#pragma mark - Private

- (void)registerPromos {
  // Add StandardPromoDisplayHandler promos here. For example:
  if (IsAppStoreRatingEnabled()) {
    _displayHandlerPromos[promos_manager::Promo::AppStoreRating] =
        [[AppStoreRatingDisplayHandler alloc] init];
  }

  // Add StandardPromoViewProvider promos here. For example:
  // TODO(crbug.com/1360880): Create first StandardPromoViewProvider promo.

  // StandardPromoAlertProvider promo(s) below:
  syncer::SyncUserSettings* syncUserSettings =
      SyncServiceFactory::GetForBrowserState(self.browser->GetBrowserState())
          ->GetUserSettings();
  _alertProviderPromos[promos_manager::Promo::PostRestoreSignInAlert] =
      [[PostRestoreSignInProvider alloc]
          initWithSyncUserSettings:syncUserSettings];
  if (GetPostRestoreDefaultBrowserPromoType() ==
      PostRestoreDefaultBrowserPromoType::kAlert) {
    _alertProviderPromos
        [promos_manager::Promo::PostRestoreDefaultBrowserAlert] =
            [[PostRestoreDefaultBrowserPromoProvider alloc] init];
  }

  // WhatsNewPromoHandler promo below:
  _displayHandlerPromos[promos_manager::Promo::WhatsNew] =
      [[WhatsNewPromoDisplayHandler alloc]
          initWithPromosManager:PromosManagerFactory::GetForBrowserState(
                                    self.browser->GetBrowserState())];

  // CredentialProvider Promo handler
  if (IsCredentialProviderExtensionPromoEnabled() || IsIOSSetUpListEnabled()) {
    _displayHandlerPromos[promos_manager::Promo::CredentialProviderExtension] =
        [[CredentialProviderPromoDisplayHandler alloc]
            initWithHandler:_credentialProviderPromoCommandHandler];
  }

  // DefaultBrowser Promo handler
  if (IsDefaultBrowserInPromoManagerEnabled()) {
    _displayHandlerPromos[promos_manager::Promo::DefaultBrowser] =
        [[DefaultBrowserPromoDisplayHandler alloc] init];
    _displayHandlerPromos[promos_manager::Promo::DefaultBrowserRemindMeLater] =
        [[DefaultBrowserRemindMeLaterPromoDisplayHandler alloc] init];
  }

  // Choice Promo handler
  if (ios::provider::IsChoiceEnabled()) {
    _displayHandlerPromos[promos_manager::Promo::Choice] =
        ios::provider::CreateChoiceDisplayHandler();
  }
}

- (PromoConfigsSet)promoImpressionLimits {
  PromoConfigsSet result;

  for (auto const& [promo, handler] : _displayHandlerPromos)
    result.emplace(handler.config);

  for (auto const& [promo, provider] : _viewProviderPromos)
    result.emplace(provider.config);

  for (auto const& [promo, banneredProvider] : _banneredViewProviderPromos)
    result.emplace(banneredProvider.config);

  for (auto const& [promo, alertProvider] : _alertProviderPromos)
    result.emplace(alertProvider.config);

  return result;
}

// Checks if `promo` is properly registered within this coordinator.
- (BOOL)isPromoUnregistered:(promos_manager::Promo)promo {
  auto handler_it = _displayHandlerPromos.find(promo);
  auto provider_it = _viewProviderPromos.find(promo);
  auto bannered_provider_it = _banneredViewProviderPromos.find(promo);
  auto alert_provider_it = _alertProviderPromos.find(promo);

  return handler_it == _displayHandlerPromos.end() &&
         provider_it == _viewProviderPromos.end() &&
         bannered_provider_it == _banneredViewProviderPromos.end() &&
         alert_provider_it == _alertProviderPromos.end();
}

@end
