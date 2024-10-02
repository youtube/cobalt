// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_default_account/consistency_default_account_view_controller.h"

#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/consistency_promo_signin/consistency_layout_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_constants.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/authentication/views/identity_view.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Margins for `self.contentView` (top, bottom, leading and trailing).
constexpr CGFloat kContentMargin = 16.;
// Space between elements in `self.contentView`.
constexpr CGFloat kContentSpacing = 16.;

}

@interface ConsistencyDefaultAccountViewController ()

// View that contains all UI elements for the view controller. This view is
// the only subview of -[ConsistencyDefaultAccountViewController view].
@property(nonatomic, strong) UIStackView* contentView;
// Button to present the default identity.
@property(nonatomic, strong) IdentityButtonControl* identityButtonControl;
// Button to
// 1. confirm the default identity and sign-in when an account is available, or
// 2. add an account when no account is available on the device.
@property(nonatomic, strong) UIButton* primaryButton;
// Title for `self.primaryButton` when it needs to show the text "Continue as…".
// This property is needed to hide the title the activity indicator is shown.
@property(nonatomic, strong) NSString* continueAsTitle;
// Activity indicator on top of `self.primaryButton`.
@property(nonatomic, strong) UIActivityIndicatorView* activityIndicatorView;
// The access point that triggered sign-in.
@property(nonatomic, assign, readonly) signin_metrics::AccessPoint accessPoint;

@end

@implementation ConsistencyDefaultAccountViewController

- (instancetype)initWithAccessPoint:(signin_metrics::AccessPoint)accessPoint {
  self = [super init];
  if (self) {
    _accessPoint = accessPoint;
  }
  return self;
}

- (void)startSpinner {
  // Add spinner.
  DCHECK(!self.activityIndicatorView);
  self.activityIndicatorView = [[UIActivityIndicatorView alloc] init];
  self.activityIndicatorView.translatesAutoresizingMaskIntoConstraints = NO;
  self.activityIndicatorView.color = [UIColor colorNamed:kSolidButtonTextColor];
  [self.primaryButton addSubview:self.activityIndicatorView];
  AddSameCenterConstraints(self.activityIndicatorView, self.primaryButton);
  [self.activityIndicatorView startAnimating];
  // Disable buttons.
  self.identityButtonControl.enabled = NO;
  self.primaryButton.enabled = NO;
  [self.primaryButton setTitle:@"" forState:UIControlStateNormal];
}

- (void)stopSpinner {
  // Remove spinner.
  DCHECK(self.activityIndicatorView);
  [self.activityIndicatorView removeFromSuperview];
  self.activityIndicatorView = nil;
  // Show the IdentityButtonControl, since it may be hidden.
  self.identityButtonControl.hidden = NO;
  // Enable buttons.
  self.identityButtonControl.enabled = YES;
  self.primaryButton.enabled = YES;
  DCHECK(self.continueAsTitle);
  [self.primaryButton setTitle:self.continueAsTitle
                      forState:UIControlStateNormal];
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  self.identityButtonControl.backgroundColor =
      [UIColor colorNamed:kGroupedSecondaryBackgroundColor];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  // Set the navigation title in the left bar button item to have left
  // alignment.
  UILabel* titleLabel = [[UILabel alloc] init];
  titleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
  titleLabel.text =
      l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_TITLE);
  titleLabel.textAlignment = NSTextAlignmentLeft;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.1;
  UIBarButtonItem* leftItem =
      [[UIBarButtonItem alloc] initWithCustomView:titleLabel];
  self.navigationItem.leftBarButtonItem = leftItem;

  NSString* skipButtonTitle;
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO) {
    skipButtonTitle = l10n_util::GetNSString(IDS_CANCEL);
  } else if (self.accessPoint == signin_metrics::AccessPoint::
                                     ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO) {
    skipButtonTitle = l10n_util::GetNSString(IDS_CLOSE);
  } else {
    skipButtonTitle = l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SKIP);
  }
  UIBarButtonItem* skipButton =
      [[UIBarButtonItem alloc] initWithTitle:skipButtonTitle
                                       style:UIBarButtonItemStylePlain
                                      target:self
                                      action:@selector(skipButtonAction:)];
  skipButton.accessibilityIdentifier =
      kWebSigninSkipButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = skipButton;
  // Replace the controller view by the scroll view.
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:scrollView];
  [NSLayoutConstraint activateConstraints:@[
    [scrollView.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [scrollView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    [scrollView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [scrollView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
  ]];
  // Create content view.
  self.contentView = [[UIStackView alloc] init];
  self.contentView.axis = UILayoutConstraintAxisVertical;
  self.contentView.distribution = UIStackViewDistributionEqualSpacing;
  self.contentView.alignment = UIStackViewAlignmentCenter;
  self.contentView.spacing = kContentSpacing;
  self.contentView.translatesAutoresizingMaskIntoConstraints = NO;
  [scrollView addSubview:self.contentView];
  UILayoutGuide* contentLayoutGuide = scrollView.contentLayoutGuide;
  UILayoutGuide* frameLayoutGuide = scrollView.safeAreaLayoutGuide;
  [NSLayoutConstraint activateConstraints:@[
    [contentLayoutGuide.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:-kContentMargin],
    [contentLayoutGuide.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:kContentMargin],
    [frameLayoutGuide.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:-kContentMargin],
    [frameLayoutGuide.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:kContentMargin],
  ]];
  // Add the label.
  UILabel* label = [[UILabel alloc] init];
  if (self.accessPoint ==
      signin_metrics::AccessPoint::ACCESS_POINT_SEND_TAB_TO_SELF_PROMO) {
    label.text =
        l10n_util::GetNSString(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_LABEL);
  } else if (self.accessPoint == signin_metrics::AccessPoint::
                                     ACCESS_POINT_NTP_FEED_CARD_MENU_PROMO) {
    label.text =
        l10n_util::GetNSString(IDS_IOS_FEED_CARD_SIGN_IN_ONLY_PROMO_LABEL);
  } else {
    label.text =
        l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_DEFAULT_ACCOUNT_LABEL);
  }

  label.textColor = [UIColor colorNamed:kGrey700Color];
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline];
  label.numberOfLines = 0;
  [self.contentView addArrangedSubview:label];
  [label.widthAnchor constraintEqualToAnchor:self.contentView.widthAnchor]
      .active = YES;
  // Add IdentityButtonControl for the default identity.
  self.identityButtonControl =
      [[IdentityButtonControl alloc] initWithFrame:CGRectZero];
  self.identityButtonControl.arrowDirection = IdentityButtonControlArrowRight;
  self.identityButtonControl.identityViewStyle = IdentityViewStyleConsistency;
  [self.identityButtonControl addTarget:self
                                 action:@selector(identityButtonControlAction:
                                                                     forEvent:)
                       forControlEvents:UIControlEventTouchUpInside];
  [self.contentView addArrangedSubview:self.identityButtonControl];
  [NSLayoutConstraint activateConstraints:@[
    [self.identityButtonControl.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ]];
  // Add the primary button (the "Continue as"/"Sign in" button).
  self.primaryButton =
      PrimaryActionButton(/* pointer_interaction_enabled */ YES);
  self.primaryButton.accessibilityIdentifier =
      kWebSigninPrimaryButtonAccessibilityIdentifier;
  self.primaryButton.translatesAutoresizingMaskIntoConstraints = NO;
  [self.primaryButton addTarget:self
                         action:@selector(primaryButtonAction:)
               forControlEvents:UIControlEventTouchUpInside];

  [self.contentView addArrangedSubview:self.primaryButton];

  [NSLayoutConstraint activateConstraints:@[
    [self.primaryButton.widthAnchor
        constraintEqualToAnchor:self.contentView.widthAnchor]
  ]];
  // Adjust the identity button control rounded corners to the same value than
  // the "continue as" button.
  self.identityButtonControl.layer.cornerRadius =
      self.primaryButton.layer.cornerRadius;

  // Ensure that keyboard is hidden.
  UIResponder* firstResponder = GetFirstResponder();
  [firstResponder resignFirstResponder];
}

#pragma mark - UI actions

- (void)skipButtonAction:(id)sender {
  [self.actionDelegate consistencyDefaultAccountViewControllerSkip:self];
}

- (void)identityButtonControlAction:(id)sender forEvent:(UIEvent*)event {
  [self.actionDelegate
      consistencyDefaultAccountViewControllerOpenIdentityChooser:self];
}

- (void)primaryButtonAction:
    (ConsistencyDefaultAccountViewController*)viewController {
  // If the IdentityButtonControl is hidden, there is no account avaiable on the
  // device.
  if (!self.identityButtonControl.hidden) {
    [self.actionDelegate
        consistencyDefaultAccountViewControllerContinueWithSelectedIdentity:
            self];
  } else {
    [self.actionDelegate
        consistencyDefaultAccountViewControllerAddAccountAndSignin:self];
  }
}

#pragma mark - ChildConsistencySheetViewController

- (CGFloat)layoutFittingHeightForWidth:(CGFloat)width {
  CGFloat contentViewWidth = width - self.view.safeAreaInsets.left -
                             self.view.safeAreaInsets.right -
                             kContentMargin * 2;
  CGSize size = CGSizeMake(contentViewWidth, 0);
  size = [self.contentView
        systemLayoutSizeFittingSize:size
      withHorizontalFittingPriority:UILayoutPriorityRequired
            verticalFittingPriority:UILayoutPriorityFittingSizeLevel];
  CGFloat safeAreaInsetsHeight = 0;
  switch (self.layoutDelegate.displayStyle) {
    case ConsistencySheetDisplayStyleBottom:
      safeAreaInsetsHeight =
          self.navigationController.view.window.safeAreaInsets.bottom;
      break;
    case ConsistencySheetDisplayStyleCentered:
      break;
  }
  // Safe area insets needs to be based on the window since the `self.view`
  // might not be part of the window hierarchy when the animation is configured.
  return self.navigationController.navigationBar.frame.size.height +
         kContentMargin + size.height + kContentMargin + safeAreaInsetsHeight;
}

#pragma mark - ConsistencyDefaultAccountConsumer

- (void)showDefaultAccountWithFullName:(NSString*)fullName
                             givenName:(NSString*)givenName
                                 email:(NSString*)email
                                avatar:(UIImage*)avatar {
  if (!self.viewLoaded) {
    // Load the view.
    [self view];
  }
  self.continueAsTitle = l10n_util::GetNSStringF(
      IDS_IOS_SIGNIN_PROMO_CONTINUE_AS, base::SysNSStringToUTF16(givenName));

  [self.identityButtonControl setIdentityName:fullName email:email];
  [self.identityButtonControl setIdentityAvatar:avatar];

  // If spinner is active, delay UI updates until stopSpinner() is called.
  if (!self.activityIndicatorView) {
    [self.primaryButton setTitle:self.continueAsTitle
                        forState:UIControlStateNormal];
    self.identityButtonControl.hidden = NO;
  }
}

- (void)hideDefaultAccount {
  if (!self.viewLoaded) {
    [self view];
  }

  // Hide the IdentityButtonControl, and update the primary button to serve as
  // a "Sign in…" button.
  self.identityButtonControl.hidden = YES;
  [self.primaryButton
      setTitle:l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_SIGN_IN)
      forState:UIControlStateNormal];
}

@end
