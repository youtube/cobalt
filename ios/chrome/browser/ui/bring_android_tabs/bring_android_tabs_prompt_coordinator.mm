// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_coordinator.h"

#import "base/check_op.h"
#import "base/notreached.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service.h"
#import "ios/chrome/browser/bring_android_tabs/bring_android_tabs_to_ios_service_factory.h"
#import "ios/chrome/browser/bring_android_tabs/features.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/bring_android_tabs_commands.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_prompt_mediator.h"
#import "ios/chrome/browser/ui/bring_android_tabs/bring_android_tabs_ui_swift.h"
#import "ios/chrome/browser/url_loading/url_loading_browser_agent.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Sets a custom radius for the half sheet presentation.
constexpr CGFloat kHalfSheetCornerRadius = 20;

// Set presentation style of a half sheet modal.
void SetModalPresentationStyle(UIViewController* view_controller) {
  if (@available(iOS 15, *)) {
    view_controller.modalPresentationStyle = UIModalPresentationPageSheet;
    UISheetPresentationController* presentation_controller =
        view_controller.sheetPresentationController;
    presentation_controller.prefersEdgeAttachedInCompactHeight = YES;
    presentation_controller.widthFollowsPreferredContentSizeWhenEdgeAttached =
        YES;
    presentation_controller.detents = @[
      UISheetPresentationControllerDetent.mediumDetent,
      UISheetPresentationControllerDetent.largeDetent,
    ];
    presentation_controller.preferredCornerRadius = kHalfSheetCornerRadius;
  } else {
    view_controller.modalPresentationStyle = UIModalPresentationFormSheet;
  }
}

}  // namespace

@implementation BringAndroidTabsPromptCoordinator {
  // Mediator that updates Chromium model objects; serves as a delegate to the
  // view controller.
  BringAndroidTabsPromptMediator* _mediator;
  // View provider for the bottom message variant of the prompt.
  BringAndroidTabsPromptBottomMessageProvider* _provider;
}

- (void)start {
  BringAndroidTabsToIOSService* service =
      BringAndroidTabsToIOSServiceFactory::GetForBrowserStateIfExists(
          self.browser->GetBrowserState());
  _mediator = [[BringAndroidTabsPromptMediator alloc]
      initWithBringAndroidTabsService:service
                            URLLoader:UrlLoadingBrowserAgent::FromBrowser(
                                          self.browser)];

  switch (GetBringYourOwnTabsPromptType()) {
    case BringYourOwnTabsPromptType::kHalfSheet: {
      BringAndroidTabsPromptConfirmationAlertViewController* confirmationAlert =
          [[BringAndroidTabsPromptConfirmationAlertViewController alloc]
              initWithTabsCount:static_cast<int>(
                                    service->GetNumberOfAndroidTabs())];
      confirmationAlert.delegate = _mediator;
      confirmationAlert.commandHandler = self.commandHandler;
      SetModalPresentationStyle(confirmationAlert);
      _viewController = confirmationAlert;
      break;
    }
    case BringYourOwnTabsPromptType::kBottomMessage: {
      _provider = [[BringAndroidTabsPromptBottomMessageProvider alloc]
          initWithTabsCount:static_cast<int>(
                                service->GetNumberOfAndroidTabs())];
      _provider.delegate = _mediator;
      _provider.commandHandler = self.commandHandler;
      _viewController = _provider.viewController;
      break;
    }
    case BringYourOwnTabsPromptType::kDisabled: {
      NOTREACHED();
      break;
    }
  }
}

- (void)stop {
  // The view controller should have already dismissed itself using the
  // Bring Android Commands handler.
  DCHECK(_viewController);
  DCHECK(_viewController.beingDismissed ||
         _viewController.parentViewController == nil);
  _viewController = nil;
  // Remove the mediator.
  DCHECK(_mediator);
  _mediator = nil;
  _provider = nil;
}

@end
