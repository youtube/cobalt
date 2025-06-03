// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_view_controller.h"

#import "base/check.h"
#import "ios/chrome/browser/infobars/badge_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_button_factory.h"
#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/browser/ui/badges/badge_item.h"
#import "ios/chrome/common/material_timing.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {

// FullScreen progress threshold in which to toggle between full screen on and
// off mode for the badge view.
const CGFloat kFullScreenProgressThreshold = 0.85;

// Spacing between the top and trailing anchors of `unreadIndicatorView` and
// `displayedBadge`.
const CGFloat kUnreadIndicatorViewSpacing = 10.0;

// Height of `unreadIndicatorView`.
const CGFloat kUnreadIndicatorViewHeight = 6.0;

// Damping ratio of animating a change to the displayed badge.
const CGFloat kUpdateDisplayedBadgeAnimationDamping = 0.85;

}  // namespace

@interface BadgeViewController ()

// Button factory.
@property(nonatomic, strong) BadgeButtonFactory* buttonFactory;

// BadgeButton to show when in FullScreen (i.e. when the
// toolbars are expanded). Setting this property will add the button to the
// StackView.
@property(nonatomic, strong) BadgeButton* displayedBadge;

// BadgeButton to show in both FullScreen and non FullScreen.
@property(nonatomic, strong) BadgeButton* fullScreenBadge;

// StackView holding the displayedBadge and fullScreenBadge.
@property(nonatomic, strong) UIStackView* stackView;

// View that displays a blue dot on the top-right corner of the displayed badge
// if there are unread badges to be shown in the overflow menu.
@property(nonatomic, strong) UIView* unreadIndicatorView;

@end

@implementation BadgeViewController

- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    DCHECK(buttonFactory);
    _buttonFactory = buttonFactory;
    _stackView = [[UIStackView alloc] init];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.stackView.translatesAutoresizingMaskIntoConstraints = NO;
  self.stackView.axis = UILayoutConstraintAxisHorizontal;
  [self.view addSubview:self.stackView];
  AddSameConstraints(self.view, self.stackView);
}

#pragma mark - Protocols

#pragma mark BadgeConsumer

- (void)setupWithDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
                fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem {
  self.displayedBadge = nil;
  self.fullScreenBadge = nil;
  if (displayedBadgeItem) {
    BadgeButton* newButton =
        [self.buttonFactory badgeButtonForBadgeType:displayedBadgeItem.badgeType
                                       usingInfoBar:nil];
    [newButton setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
                  animated:NO];
    self.displayedBadge = newButton;
  }
  if (fullscreenBadgeItem) {
    self.fullScreenBadge = [self.buttonFactory
        badgeButtonForBadgeType:fullscreenBadgeItem.badgeType
                   usingInfoBar:nil];
  }
}

- (void)updateDisplayedBadge:(id<BadgeItem>)displayedBadgeItem
             fullScreenBadge:(id<BadgeItem>)fullscreenBadgeItem
                     infoBar:(InfoBarIOS*)infoBar {
  if (fullscreenBadgeItem) {
    if (!self.fullScreenBadge ||
        self.fullScreenBadge.badgeType != fullscreenBadgeItem.badgeType) {
      BadgeButton* newButton = [self.buttonFactory
          badgeButtonForBadgeType:fullscreenBadgeItem.badgeType
                     usingInfoBar:nil];
      self.fullScreenBadge = newButton;
    }
  } else {
    self.fullScreenBadge = nil;
  }

  if (displayedBadgeItem) {
    if (self.displayedBadge &&
        self.displayedBadge.badgeType == displayedBadgeItem.badgeType) {
      [self.displayedBadge
          setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
             animated:YES];
    } else {
      BadgeButton* newButton = [self.buttonFactory
          badgeButtonForBadgeType:displayedBadgeItem.badgeType
                     usingInfoBar:infoBar];
      [newButton setAccepted:displayedBadgeItem.badgeState & BadgeStateAccepted
                    animated:NO];
      self.displayedBadge = newButton;
    }
    // Disable button if banner is being displayed.
    [self.displayedBadge
        setEnabled:!(displayedBadgeItem.badgeState & BadgeStatePresented)];
  } else {
    self.displayedBadge = nil;
  }
}

- (void)markDisplayedBadgeAsRead:(BOOL)read {
  // Lazy init if the unread indicator needs to be shown.
  if (!self.unreadIndicatorView && !read) {
    // Add unread indicator to the displayed badge.
    self.unreadIndicatorView = [[UIView alloc] init];
    self.unreadIndicatorView.layer.cornerRadius =
        kUnreadIndicatorViewHeight / 2;
    self.unreadIndicatorView.backgroundColor = [UIColor colorNamed:kBlueColor];
    self.unreadIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
    self.unreadIndicatorView.accessibilityIdentifier =
        kBadgeUnreadIndicatorAccessibilityIdentifier;
    [_displayedBadge addSubview:self.unreadIndicatorView];
    [NSLayoutConstraint activateConstraints:@[
      [self.unreadIndicatorView.trailingAnchor
          constraintEqualToAnchor:_displayedBadge.trailingAnchor
                         constant:-kUnreadIndicatorViewSpacing],
      [self.unreadIndicatorView.topAnchor
          constraintEqualToAnchor:_displayedBadge.topAnchor
                         constant:kUnreadIndicatorViewSpacing],
      [self.unreadIndicatorView.heightAnchor
          constraintEqualToConstant:kUnreadIndicatorViewHeight],
      [self.unreadIndicatorView.heightAnchor
          constraintEqualToAnchor:self.unreadIndicatorView.widthAnchor]
    ]];
  }
  if (self.unreadIndicatorView) {
    self.unreadIndicatorView.hidden = read;
  }
}

#pragma mark FullscreenUIElement

- (void)updateForFullscreenProgress:(CGFloat)progress {
  BOOL badgeViewShouldCollapse = progress <= kFullScreenProgressThreshold;
  if (badgeViewShouldCollapse) {
    self.fullScreenBadge.fullScreenOn = YES;
    // Fade in/out in the FullScreen badge with the FullScreen on
    // configurations.
    CGFloat alphaValue = fmax((kFullScreenProgressThreshold - progress) /
                                  kFullScreenProgressThreshold,
                              0);
    self.fullScreenBadge.alpha = alphaValue;
    self.displayedBadge.hidden = YES;
  } else {
    self.fullScreenBadge.fullScreenOn = NO;
    // Fade in/out the FullScreen badge with the FullScreen off configurations
    // at a speed matching that of the trailing button in the
    // LocationBarSteadyView.
    CGFloat alphaValue = fmax((progress - kFullScreenProgressThreshold) /
                                  (1 - kFullScreenProgressThreshold),
                              0);
    self.displayedBadge.hidden = NO;
    self.displayedBadge.alpha = alphaValue;
    self.fullScreenBadge.alpha = alphaValue;
  }
}

#pragma mark - Getter/Setter

- (void)setDisplayedBadge:(BadgeButton*)badgeButton {
  if (badgeButton.badgeType == self.displayedBadge.badgeType) {
    return;
  }

  [self.stackView removeArrangedSubview:_displayedBadge];
  [_displayedBadge removeFromSuperview];
  if (!badgeButton) {
    _displayedBadge = nil;
    self.unreadIndicatorView = nil;
    return;
  }
  _displayedBadge = badgeButton;

  // Configure the initial state of the animation.
  self.view.alpha = 0;
  self.view.transform = CGAffineTransformMakeScale(0.1, 0.1);
  [self.stackView addArrangedSubview:_displayedBadge];
  [UIView animateWithDuration:kMaterialDuration2
                        delay:0
       usingSpringWithDamping:kUpdateDisplayedBadgeAnimationDamping
        initialSpringVelocity:0
                      options:UIViewAnimationOptionBeginFromCurrentState
                   animations:^{
                     self.view.alpha = 1;
                     self.view.transform = CGAffineTransformIdentity;
                   }
                   completion:nil];
}

- (void)setFullScreenBadge:(BadgeButton*)fullScreenBadge {
  [self.stackView removeArrangedSubview:_fullScreenBadge];
  [_fullScreenBadge removeFromSuperview];
  if (!fullScreenBadge) {
    _fullScreenBadge = nil;
    return;
  }
  _fullScreenBadge = fullScreenBadge;
  [self.stackView insertArrangedSubview:_fullScreenBadge atIndex:0];
}

@end
