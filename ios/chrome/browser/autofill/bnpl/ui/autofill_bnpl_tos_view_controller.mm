// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/bnpl/ui/autofill_bnpl_tos_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/common/ui/button_stack/button_stack_configuration.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// The height of the issuer logo in the header.
constexpr CGFloat kIssuerLogoHeight = 24.0;

// The vertical spacing between elements in the terms stack view.
constexpr CGFloat kVerticalSpacing = 16.0;

// The horizontal spacing between the icon and the text label in a bullet row.
constexpr CGFloat kRowHorizontalSpacing = 12.0;

// The width and height of the bullet point icons.
constexpr CGFloat kBulletIconSize = 20.0;

}  // namespace

@implementation AutofillBnplTosBulletItem
@end

@interface AutofillBnplTosViewController () <ConfirmationAlertActionHandler,
                                             UITextViewDelegate>
@end

@implementation AutofillBnplTosViewController {
  UIImage* _issuerLogo;
  NSArray<AutofillBnplTosBulletItem*>* _bulletPoints;
  NSAttributedString* _consentText;

  UIStackView* _underTitleStack;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    self.topAlignedLayout = YES;
    self.imageHasFixedSize = YES;
    self.titleTextStyle = UIFontTextStyleTitle2;
    self.shouldFillInformationStack = YES;
    self.configuration.primaryActionString =
        l10n_util::GetNSString(IDS_CONTINUE);
    self.configuration.secondaryActionString =
        l10n_util::GetNSString(IDS_CANCEL);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.actionHandler = self;
  [self setupAboveTitleView];
  [self setupUnderTitleView];
  [super viewDidLoad];
}

#pragma mark - AutofillBnplTosConsumer

- (void)setTitleText:(NSString*)titleText {
  self.titleString = titleText;
}

- (void)setIssuerLogo:(UIImage*)issuerLogo {
  _issuerLogo = issuerLogo;
  if (self.isViewLoaded) {
    // TODO(crbug.com/521882618): Handle dynamic updates of the logo after
    // viewDidLoad when the mediator is implemented.
    [self setupAboveTitleView];
  }
}

- (void)setTermsBulletPoints:
    (NSArray<AutofillBnplTosBulletItem*>*)bulletPoints {
  _bulletPoints = [bulletPoints copy];
  if (self.isViewLoaded) {
    // TODO(crbug.com/521882618): Handle dynamic updates of the terms after
    // viewDidLoad when the mediator is implemented.
    [self setupUnderTitleView];
  }
}

- (void)setConsentText:(NSAttributedString*)consentText {
  _consentText = [consentText copy];
  self.subtitleString = consentText.string;
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  subtitle.attributedText = _consentText;
  subtitle.delegate = self;
  subtitle.selectable = YES;
  subtitle.userInteractionEnabled = YES;
  subtitle.scrollEnabled = NO;
  subtitle.textContainerInset = UIEdgeInsetsZero;
  subtitle.textContainer.lineFragmentPadding = 0;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  [self.mutator didTapContinue];
}

- (void)confirmationAlertSecondaryAction {
  [self.mutator didTapCancel];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange {
  [self.delegate tosViewController:self didTapOnURL:URL];
  return NO;
}

#pragma mark - Private Layout Helpers

// Configures Issuer logo above the title.
- (void)setupAboveTitleView {
  if (!_issuerLogo) {
    return;
  }

  UIImageView* issuerView = [[UIImageView alloc] initWithImage:_issuerLogo];
  issuerView.contentMode = UIViewContentModeScaleAspectFit;
  issuerView.translatesAutoresizingMaskIntoConstraints = NO;

  [NSLayoutConstraint activateConstraints:@[
    [issuerView.heightAnchor constraintEqualToConstant:kIssuerLogoHeight],
  ]];

  self.aboveTitleView = issuerView;
}

// Creates a horizontal stack view row for a single terms bullet point.
- (UIView*)createBulletRowWithItem:(AutofillBnplTosBulletItem*)item {
  UIStackView* row = [[UIStackView alloc] init];
  row.axis = UILayoutConstraintAxisHorizontal;
  row.alignment = UIStackViewAlignmentTop;
  row.distribution = UIStackViewDistributionFill;
  row.spacing = kRowHorizontalSpacing;

  UIImageView* iconView = [[UIImageView alloc] initWithImage:item.icon];
  iconView.contentMode = UIViewContentModeScaleAspectFit;
  iconView.translatesAutoresizingMaskIntoConstraints = NO;
  iconView.tintColor = [UIColor colorNamed:kTextPrimaryColor];

  UITextView* textView = [[UITextView alloc] init];
  textView.attributedText = item.text;
  textView.selectable = YES;
  textView.scrollEnabled = NO;
  textView.editable = NO;
  textView.delegate = self;
  textView.adjustsFontForContentSizeCategory = YES;
  textView.textContainerInset = UIEdgeInsetsZero;
  textView.textContainer.lineFragmentPadding = 0;
  textView.textColor = [UIColor colorNamed:kTextPrimaryColor];
  textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  textView.backgroundColor = [UIColor clearColor];
  textView.translatesAutoresizingMaskIntoConstraints = NO;

  [row addArrangedSubview:iconView];
  [row addArrangedSubview:textView];

  [NSLayoutConstraint activateConstraints:@[
    [iconView.widthAnchor constraintEqualToConstant:kBulletIconSize],
    [iconView.heightAnchor constraintEqualToAnchor:iconView.widthAnchor],
  ]];

  return row;
}

// Configures the list of terms bullet points under the title.
- (void)setupUnderTitleView {
  if (!_bulletPoints.count) {
    return;
  }

  if (_underTitleStack) {
    [_underTitleStack removeFromSuperview];
  }

  _underTitleStack = [[UIStackView alloc] init];
  _underTitleStack.axis = UILayoutConstraintAxisVertical;
  _underTitleStack.alignment = UIStackViewAlignmentFill;
  _underTitleStack.distribution = UIStackViewDistributionFill;
  _underTitleStack.spacing = kVerticalSpacing;
  _underTitleStack.translatesAutoresizingMaskIntoConstraints = NO;

  for (AutofillBnplTosBulletItem* item in _bulletPoints) {
    UIView* row = [self createBulletRowWithItem:item];
    [_underTitleStack addArrangedSubview:row];
  }

  self.underTitleView = _underTitleStack;
}

@end
