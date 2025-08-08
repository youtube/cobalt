// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view.h"

#import <UIKit/UIKit.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/features.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_util.h"
#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_view_label.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Button content padding (Vertical and Horizontal).
const CGFloat kButtonPaddingV = 15.0f;
const CGFloat kButtonPaddingH = 38.0f;
// Max radius for the authenticate button background.
const CGFloat kAuthenticateButtonBagroundMaxCornerRadius = 30.0f;
// Distance from top and bottom to content (buttons/logos).
const CGFloat kVerticalContentPadding = 70.0f;
}  // namespace

@interface IncognitoReauthView () <IncognitoReauthViewLabelOwner>
@end

@implementation IncognitoReauthView {
  // The background view for the authenticate button.
  // Has to be separate from the button because it's a blur view (on iOS 13+).
  UIView* _authenticateButtonBackgroundView;
  // Use a IncognitoReauthViewLabel for the button label, because the built-in
  // UIButton's `titleLabel` does not correctly resize for multiline labels and
  // using a UILabel doesn't provide feedback to adjust the corner radius.
  IncognitoReauthViewLabel* _authenticateButtonLabel;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    // Increase blur intensity by layering some blur views to make
    // content behind really not recognizeable.
    for (int i = 0; i < 3; i++) {
      UIBlurEffect* blurEffect =
          [UIBlurEffect effectWithStyle:UIBlurEffectStyleLight];
      UIVisualEffectView* blurView =
          [[UIVisualEffectView alloc] initWithEffect:blurEffect];
      [self addSubview:blurView];
      blurView.translatesAutoresizingMaskIntoConstraints = NO;
      AddSameConstraints(self, blurView);
    }

    UIBlurEffect* blurEffect =
        [UIBlurEffect effectWithStyle:UIBlurEffectStyleDark];
    UIVisualEffectView* blurBackgroundView =
        [[UIVisualEffectView alloc] initWithEffect:blurEffect];
    [self addSubview:blurBackgroundView];
    blurBackgroundView.translatesAutoresizingMaskIntoConstraints = NO;
    AddSameConstraints(self, blurBackgroundView);

    UIImage* incognitoLogo = CustomSymbolWithPointSize(kIncognitoSymbol, 28);
    _logoView = [[UIImageView alloc] initWithImage:incognitoLogo];
    _logoView.tintColor = UIColor.whiteColor;
    _logoView.translatesAutoresizingMaskIntoConstraints = NO;
    [blurBackgroundView.contentView addSubview:_logoView];
    AddSameCenterXConstraint(_logoView, blurBackgroundView);
    [_logoView.topAnchor
        constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide.topAnchor
                       constant:kVerticalContentPadding]
        .active = YES;

    _tabSwitcherButton = [[UIButton alloc] init];
    _tabSwitcherButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_tabSwitcherButton setTitleColor:[UIColor whiteColor]
                             forState:UIControlStateNormal];
    [_tabSwitcherButton setTitleColor:[UIColor colorWithWhite:1 alpha:0.4]
                             forState:UIControlStateHighlighted];

    [_tabSwitcherButton setTitle:l10n_util::GetNSString(
                                     IDS_IOS_INCOGNITO_REAUTH_GO_TO_NORMAL_TABS)
                        forState:UIControlStateNormal];
    _tabSwitcherButton.titleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
    _tabSwitcherButton.titleLabel.adjustsFontSizeToFitWidth = YES;
    _tabSwitcherButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _tabSwitcherButton.pointerInteractionEnabled = YES;

    UIView* authButtonContainer =
        [self buildAuthenticateButtonWithBlurEffect:blurEffect];
    [blurBackgroundView.contentView addSubview:authButtonContainer];
    AddSameCenterConstraints(blurBackgroundView, authButtonContainer);
    _authenticateButtonBackgroundView = authButtonContainer;

    [blurBackgroundView.contentView addSubview:_tabSwitcherButton];
    AddSameCenterXConstraint(_tabSwitcherButton, blurBackgroundView);

    if (IsIOSSoftLockEnabled()) {
      _exitIncognitoButton = [[UIButton alloc] init];
      _exitIncognitoButton.translatesAutoresizingMaskIntoConstraints = NO;
      [_exitIncognitoButton setTitleColor:[UIColor whiteColor]
                                 forState:UIControlStateNormal];
      [_exitIncognitoButton setTitleColor:[UIColor colorWithWhite:1 alpha:0.4]
                                 forState:UIControlStateHighlighted];
      // TODO: Check the capitalization of "Incognito Tabs". Match the current
      // implementation of the 2 other buttons.
      [_exitIncognitoButton
          setTitle:l10n_util::GetNSString(
                       IDS_IOS_INCOGNITO_REAUTH_CLOSE_INCOGNITO_TABS)
          forState:UIControlStateNormal];
      _exitIncognitoButton.titleLabel.font =
          [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
      _exitIncognitoButton.titleLabel.adjustsFontSizeToFitWidth = YES;
      _exitIncognitoButton.titleLabel.adjustsFontForContentSizeCategory = YES;
      _exitIncognitoButton.pointerInteractionEnabled = YES;

      [blurBackgroundView.contentView addSubview:_exitIncognitoButton];
      AddSameCenterXConstraint(_exitIncognitoButton, blurBackgroundView);

      [NSLayoutConstraint activateConstraints:@[
        [_exitIncognitoButton.topAnchor
            constraintEqualToAnchor:authButtonContainer.bottomAnchor
                           constant:kButtonPaddingV],
        [_exitIncognitoButton.widthAnchor
            constraintLessThanOrEqualToAnchor:self.widthAnchor
                                     constant:-2 * kButtonPaddingH],
      ]];
    }

    [NSLayoutConstraint activateConstraints:@[
      [_tabSwitcherButton.topAnchor
          constraintEqualToAnchor:blurBackgroundView.safeAreaLayoutGuide
                                      .bottomAnchor
                         constant:-kVerticalContentPadding],
      [_authenticateButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
      [_tabSwitcherButton.widthAnchor
          constraintLessThanOrEqualToAnchor:self.widthAnchor
                                   constant:-2 * kButtonPaddingH],
    ]];

    if (@available(iOS 17, *)) {
      NSArray<UITrait>* traits = TraitCollectionSetForTraits(nil);
      __weak IncognitoReauthView* weakSelf = self;
      [self registerForTraitChanges:traits
                        withHandler:^(id<UITraitEnvironment> traitEnvironment,
                                      UITraitCollection* previousCollection) {
                          [weakSelf setNeedsLayout];
                          [weakSelf layoutIfNeeded];
                        }];
    }

    [self setNeedsLayout];
    [self layoutIfNeeded];
  }

  return self;
}

#if !defined(__IPHONE_17_0) || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_17_0
- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (@available(iOS 17, *)) {
    return;
  }

  [self setNeedsLayout];
  [self layoutIfNeeded];
}
#endif

// Creates _authenticateButton.
// Returns a "decoration" pill-shaped view containing _authenticateButton.
- (UIView*)buildAuthenticateButtonWithBlurEffect:(UIBlurEffect*)blurEffect {
  DCHECK(!_authenticateButton);

  _authenticateButtonLabel = [[IncognitoReauthViewLabel alloc] init];
  _authenticateButtonLabel.owner = self;
  _authenticateButtonLabel.numberOfLines = 0;
  _authenticateButtonLabel.textColor = [UIColor colorWithWhite:1 alpha:0.95];
  _authenticateButtonLabel.textAlignment = NSTextAlignmentCenter;
  _authenticateButtonLabel.adjustsFontForContentSizeCategory = YES;
  _authenticateButtonLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2];
  _authenticateButtonLabel.text = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  [_authenticateButtonLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [_authenticateButtonLabel
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisVertical];
  _authenticateButtonLabel.translatesAutoresizingMaskIntoConstraints = NO;
  // Disable a11y; below the UIButton will get a correct label.
  _authenticateButtonLabel.accessibilityLabel = nil;

  UIButton* button = [[UIButton alloc] init];
  button.backgroundColor = [UIColor clearColor];

  button.accessibilityLabel = l10n_util::GetNSStringF(
      IDS_IOS_INCOGNITO_REAUTH_UNLOCK_BUTTON_VOICEOVER_LABEL,
      base::SysNSStringToUTF16(BiometricAuthenticationTypeString()));
  button.translatesAutoresizingMaskIntoConstraints = NO;

  button.pointerInteractionEnabled = YES;

  UIView* backgroundView = nil;
  UIVisualEffectView* effectView = [[UIVisualEffectView alloc]
      initWithEffect:[UIVibrancyEffect
                         effectForBlurEffect:blurEffect
                                       style:UIVibrancyEffectStyleFill]];

  [button addSubview:_authenticateButtonLabel];
  [effectView.contentView addSubview:button];
  backgroundView = effectView;
  AddSameConstraintsWithInsets(
      button, _authenticateButtonLabel,
      NSDirectionalEdgeInsetsMake(-kButtonPaddingV, -kButtonPaddingH,
                                  -kButtonPaddingV, -kButtonPaddingH));

  backgroundView.backgroundColor =
      [IncognitoReauthView blurButtonBackgroundColor];
  backgroundView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(backgroundView, button);

  // Handle touch up and down events to create a "highlight" state.
  // The normal button highlight state is not usable here because the actual
  // button is transparent.
  [button addTarget:self
                action:@selector(blurButtonEventHandler)
      forControlEvents:UIControlEventAllEvents];

  _authenticateButton = button;

  return backgroundView;
}

#pragma mark - public

- (void)setAuthenticateButtonText:(NSString*)text
               accessibilityLabel:(NSString*)accessibilityLabel {
  _authenticateButtonLabel.text = text;
  self.authenticateButton.accessibilityLabel = accessibilityLabel;
}

#pragma mark - voiceover

- (BOOL)accessibilityViewIsModal {
  return YES;
}

- (BOOL)accessibilityPerformMagicTap {
  [self.authenticateButton
      sendActionsForControlEvents:UIControlEventTouchUpInside];
  return YES;
}

#pragma mark - internal

- (void)blurButtonEventHandler {
  UIView* buttonBackgroundView = _authenticateButtonBackgroundView;
  UIColor* newColor =
      [self.authenticateButton isHighlighted]
          ? [IncognitoReauthView blurButtonHighlightBackgroundColor]
          : [IncognitoReauthView blurButtonBackgroundColor];

  [UIView animateWithDuration:0.1
                   animations:^{
                     buttonBackgroundView.backgroundColor = newColor;
                   }];
}

#pragma mark - IncognitoReauthViewLabelOwner

- (void)labelDidLayout {
  CGFloat cornerRadius =
      std::min(kAuthenticateButtonBagroundMaxCornerRadius,
               _authenticateButtonBackgroundView.frame.size.height / 2);
  _authenticateButtonBackgroundView.layer.cornerRadius = cornerRadius;
}

#pragma mark - helpers

+ (UIColor*)blurButtonBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.15];
}

+ (UIColor*)blurButtonHighlightBackgroundColor {
  return [UIColor colorWithWhite:1 alpha:0.6];
}

@end
