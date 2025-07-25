// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller.h"

#import "base/notreached.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_view_controller_delegate.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace {
// Name of the parcel tracking icon.
NSString* const kOptInIcon = @"parcel_tracking_icon_new";
// Radius size of the table view.
CGFloat const kTableViewCornerRadius = 10;
// Horizontal margin for the view.
CGFloat const kHorizontalMargin = 24;
// Spacing before the image.
CGFloat const kSpacingBeforeImage = 23;
// Size of the radio buttons.
CGFloat const kRadioButtonSize = 20;
}  // namespace

@interface ParcelTrackingOptInViewController () <UITableViewDelegate,
                                                 UITextViewDelegate,
                                                 UITableViewDataSource>

@end

@implementation ParcelTrackingOptInViewController {
  UITableView* _tableView;
  IOSParcelTrackingOptInStatus _selection;
  NSLayoutConstraint* _optionsViewHeightConstraint;
}

- (void)viewDidLoad {
  UIView* optionsView = [self createOptionsView];
  self.underTitleView = optionsView;
  self.titleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_TITLE);
  self.primaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_ENABLE_TRACKING);
  self.secondaryActionString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SECONDARY_ACTION);
  self.titleTextStyle = UIFontTextStyleTitle2;
  self.subtitleString =
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SUBTITLE);
  self.subtitleTextStyle = UIFontTextStyleCallout;
  self.showDismissBarButton = NO;
  self.image = [UIImage imageNamed:kOptInIcon];
  self.imageHasFixedSize = true;
  self.topAlignedLayout = YES;
  self.customSpacingAfterImage = 0;
  self.customSpacingBeforeImageIfNoNavigationBar = kSpacingBeforeImage;
  [super viewDidLoad];

  // Assign table view's width anchor now that it is in the same hierarchy as
  // the top view.
  [NSLayoutConstraint activateConstraints:@[
    [optionsView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                              constant:kHorizontalMargin],
    [optionsView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                               constant:-kHorizontalMargin],
  ]];
  [self updateButtonForState:UIControlStateDisabled];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  [self updateOptionsViewHeightConstraint];
  self.view.directionalLayoutMargins =
      NSDirectionalEdgeInsetsMake(0, kHorizontalMargin, 0, kHorizontalMargin);
}

#pragma mark - ConfirmationAlertViewController

- (void)customizeSubtitle:(UITextView*)subtitle {
  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSParagraphStyleAttributeName : paragraphStyle,
  };
  NSDictionary* linkAttributes = @{
    NSLinkAttributeName : net::NSURLWithGURL(GURL("chrome://settings")),
    NSUnderlineStyleAttributeName : @(NSUnderlineStyleSingle)
  };
  subtitle.attributedText = AttributedStringFromStringWithLink(
      l10n_util::GetNSString(IDS_IOS_PARCEL_TRACKING_OPT_IN_SUBTITLE),
      textAttributes, linkAttributes);
  subtitle.delegate = self;
  subtitle.selectable = YES;
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  switch (_selection) {
    case IOSParcelTrackingOptInStatus::kAlwaysTrack:
      [self.delegate alwaysTrackTapped];
      break;
    case IOSParcelTrackingOptInStatus::kAskToTrack:
      [self.delegate askToTrackTapped];
      break;
    case IOSParcelTrackingOptInStatus::kNeverTrack:
      NOTREACHED();
      break;
  }
}

- (void)confirmationAlertSecondaryAction {
  [self.delegate noThanksTapped];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  [self.delegate parcelTrackingSettingsPageLinkTapped];
  return NO;
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 2;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewTextCell* cell =
      [tableView dequeueReusableCellWithIdentifier:@"cell"];

  NSString* title = indexPath.row == 0
                        ? l10n_util::GetNSString(
                              IDS_IOS_PARCEL_TRACKING_OPT_IN_PRIMARY_ACTION)
                        : l10n_util::GetNSString(
                              IDS_IOS_PARCEL_TRACKING_OPT_IN_TERTIARY_ACTION);
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = YES;
  cell.textLabel.text = title;
  cell.isAccessibilityElement = YES;
  cell.accessibilityLabel = cell.textLabel.text;
  cell.accessibilityTraits =
      [self accessibilityTraitsForButton:/*selected=*/NO];

  cell.accessoryView =
      [[UIImageView alloc] initWithImage:DefaultSymbolTemplateWithPointSize(
                                             kCircleSymbol, kRadioButtonSize)];
  cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];

  // Make separator invisible on second cell.
  if (indexPath.row > 0) {
    cell.separatorInset =
        UIEdgeInsetsMake(0.f, tableView.frame.size.width, 0.f, 0.f);
  }

  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  _selection = indexPath.row == 0 ? IOSParcelTrackingOptInStatus::kAlwaysTrack
                                  : IOSParcelTrackingOptInStatus::kAskToTrack;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  UIImage* icon;
  if (@available(iOS 16.0, *)) {
    icon = DefaultSymbolWithPointSize(kButtonProgrammableSymbol,
                                      kSymbolAccessoryPointSize);
  } else {
    icon = DefaultSymbolWithPointSize(kCircleCircleFillSymbol,
                                      kSymbolAccessoryPointSize);
  }
  cell.accessoryView = [[UIImageView alloc] initWithImage:icon];
  cell.accessoryView.tintColor = [UIColor colorNamed:kBlueColor];
  [self updateButtonForState:UIControlStateNormal];
  cell.accessibilityTraits =
      [self accessibilityTraitsForButton:/*selected=*/YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryView = [[UIImageView alloc]
      initWithImage:DefaultSymbolTemplateWithPointSize(
                        kCircleSymbol, kSymbolAccessoryPointSize)];
  cell.accessoryView.tintColor = [UIColor colorNamed:kGrey500Color];
  cell.accessibilityTraits =
      [self accessibilityTraitsForButton:/*selected=*/NO];
}

#pragma mark - Private

// Creates the view with the "always track" and "ask to track" options.
- (UITableView*)createOptionsView {
  _tableView = [[UITableView alloc] initWithFrame:CGRectZero
                                            style:UITableViewStylePlain];
  _tableView.layer.cornerRadius = kTableViewCornerRadius;
  _tableView.estimatedRowHeight = UITableViewAutomaticDimension;
  _tableView.scrollEnabled = NO;
  _tableView.showsVerticalScrollIndicator = NO;
  _tableView.delegate = self;
  _tableView.userInteractionEnabled = YES;
  _tableView.translatesAutoresizingMaskIntoConstraints = NO;
  _tableView.dataSource = self;
  _tableView.separatorInset = UIEdgeInsetsZero;
  [_tableView registerClass:TableViewTextCell.class
      forCellReuseIdentifier:@"cell"];
  _optionsViewHeightConstraint =
      [_tableView.heightAnchor constraintEqualToConstant:0];
  _optionsViewHeightConstraint.active = YES;

  return _tableView;
}

// Updates the "Enable Tracking" button. The button should be disabled initially
// and only enabled after an option, either "always track" or "ask to track",
// has been selected by the user.
- (void)updateButtonForState:(UIControlState)state {
  UIButton* button = self.primaryActionButton;
  if (state == UIControlStateDisabled) {
    button.userInteractionEnabled = NO;
    [button setBackgroundColor:[UIColor colorNamed:kGrey200Color]];
    [button setTitleColor:[UIColor colorNamed:kGrey600Color]
                 forState:UIControlStateNormal];
  } else if (state == UIControlStateNormal) {
    button.userInteractionEnabled = YES;
    [button setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    [button setTitleColor:[UIColor colorNamed:kBackgroundColor]
                 forState:UIControlStateNormal];
  }
}

// Updates the optionsView's height constraint.
- (void)updateOptionsViewHeightConstraint {
  CGFloat totalCellHeight = 0;
  for (UITableViewCell* cell in _tableView.visibleCells) {
    totalCellHeight += cell.frame.size.height;
  }
  _optionsViewHeightConstraint.constant = totalCellHeight;
}

// Returns the accessibility traits for the radio button options. `selected`
// should be true if the radio button is selected.
- (UIAccessibilityTraits)accessibilityTraitsForButton:(BOOL)selected {
  UIAccessibilityTraits accessibilityTraits = UIAccessibilityTraitButton;
  if (selected) {
    accessibilityTraits |= UIAccessibilityTraitSelected;
  }
  return accessibilityTraits;
}

@end
