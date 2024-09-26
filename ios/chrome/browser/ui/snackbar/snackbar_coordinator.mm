// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/snackbar/snackbar_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SnackbarCoordinator ()

@property(nonatomic, weak) id<SnackbarCoordinatorDelegate> delegate;

@end

@implementation SnackbarCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                                   browser:(Browser*)browser
                                  delegate:(id<SnackbarCoordinatorDelegate>)
                                               delegate {
  DCHECK(delegate);

  self = [super initWithBaseViewController:baseViewController browser:browser];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

- (void)start {
  DCHECK(self.browser);

  // Set the font which supports the Dynamic Type.
  UIFont* defaultSnackbarFont =
      [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
  [[MDCSnackbarManager defaultManager] setMessageFont:defaultSnackbarFont];
  [[MDCSnackbarManager defaultManager] setButtonFont:defaultSnackbarFont];

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher startDispatchingToTarget:self
                           forProtocol:@protocol(SnackbarCommands)];
}

- (void)stop {
  DCHECK(self.browser);
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  [dispatcher stopDispatchingToTarget:self];
}

#pragma mark - SnackbarCommands

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message {
  CGFloat offset = [self.delegate
      snackbarCoordinatorBottomOffsetForCurrentlyPresentedView:self];
  [self showSnackbarMessage:message bottomOffset:offset];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
             withHapticType:(UINotificationFeedbackType)type {
  TriggerHapticFeedbackForNotification(type);
  [self showSnackbarMessage:message];
}

- (void)showSnackbarMessage:(MDCSnackbarMessage*)message
               bottomOffset:(CGFloat)offset {
  [[MDCSnackbarManager defaultManager]
      setPresentationHostView:self.baseViewController.view.window];
  [[MDCSnackbarManager defaultManager] setBottomOffset:offset];
  [[MDCSnackbarManager defaultManager] showMessage:message];
}

- (void)showSnackbarWithMessage:(NSString*)messageText
                     buttonText:(NSString*)buttonText
                  messageAction:(void (^)(void))messageAction
               completionAction:(void (^)(BOOL))completionAction {
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.handler = messageAction;
  action.title = buttonText;
  action.accessibilityLabel = buttonText;
  MDCSnackbarMessage* message =
      [MDCSnackbarMessage messageWithText:messageText];
  message.action = action;
  message.completionHandler = completionAction;

  [self showSnackbarMessage:message];
}

@end
