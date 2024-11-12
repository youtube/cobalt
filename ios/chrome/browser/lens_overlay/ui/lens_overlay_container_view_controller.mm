// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_container_view_controller.h"

#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/lens_overlay/ui/lens_overlay_accessibility_identifier_constants.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

@implementation LensOverlayContainerViewController {
  // The overlay commands handler.
  id<LensOverlayCommands> _overlayCommandsHandler;
  // The overlay close button.
  UIButton* _closeButton;
  // View that blocks user interaction with selection UI when the consent view
  // controller is displayed. Note that selection UI isn't started, so it won't
  // accept many interactions, but we do this to be extra safe.
  UIView* _selectionInteractionBlockingView;
}

- (instancetype)initWithLensOverlayCommandsHandler:
    (id<LensOverlayCommands>)handler {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _overlayCommandsHandler = handler;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor clearColor];
  self.view.accessibilityIdentifier = kLenscontainerViewAccessibilityIdentifier;

  if (!self.selectionViewController) {
    return;
  }

  [self addChildViewController:self.selectionViewController];
  [self.view addSubview:self.selectionViewController.view];

  self.selectionViewController.view.translatesAutoresizingMaskIntoConstraints =
      NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.selectionViewController.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor],
    [self.selectionViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [self.selectionViewController.view.leftAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leftAnchor],
    [self.selectionViewController.view.rightAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.rightAnchor],
  ]];

  [self.selectionViewController didMoveToParentViewController:self];
}

- (UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskPortrait;
}

- (BOOL)selectionInteractionDisabled {
  return _selectionInteractionBlockingView != nil;
}

- (void)setSelectionInteractionDisabled:(BOOL)selectionInteractionDisabled {
  if (!selectionInteractionDisabled) {
    [_selectionInteractionBlockingView removeFromSuperview];
    _selectionInteractionBlockingView = nil;
    return;
  }

  if (_selectionInteractionBlockingView) {
    return;
  }

  UIView* blocker = [[UIView alloc] init];
  blocker.backgroundColor = UIColor.clearColor;
  blocker.userInteractionEnabled = YES;
  [self.view addSubview:blocker];
  blocker.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.view, blocker);
  _selectionInteractionBlockingView = blocker;
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  [self closeOverlayRequested];
  return YES;
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  [self escapeButtonPressed];
}

#pragma mark - Actions

- (void)closeOverlayRequested {
  [_overlayCommandsHandler destroyLensUI:YES
                                  reason:lens::LensOverlayDismissalSource::
                                             kAccessibilityEscapeGesture];
}

- (void)escapeButtonPressed {
  [self closeOverlayRequested];
}

@end
