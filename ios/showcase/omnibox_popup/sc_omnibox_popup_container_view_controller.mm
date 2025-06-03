// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_container_view_controller.h"

#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"

namespace {
CGFloat kFakeImageWidth = 30;
CGFloat kFakeSpacing = 16;
CGFloat kFakeImageLeadingSpacing = 15;
CGFloat kFakeImageToTextSpacing = 14;
CGFloat kFakeTextBoxWidth = 240;
}  // namespace

@implementation SCOmniboxPopupContainerViewController

- (instancetype)initWithPopupViewController:
    (OmniboxPopupViewController*)popupViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _popupViewController = popupViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // The omnibox popup uses layout guides from the omnibox container view to
  // position its elements. Showcase needs to set up these layout guides as well
  // so the positioning can be correct.
  UIView* fakeImageView = [[UIView alloc] initWithFrame:CGRectZero];
  fakeImageView.translatesAutoresizingMaskIntoConstraints = NO;
  fakeImageView.backgroundColor = UIColor.blueColor;
  UIView* fakeTextView = [[UIView alloc] initWithFrame:CGRectZero];
  fakeTextView.translatesAutoresizingMaskIntoConstraints = NO;
  fakeTextView.backgroundColor = UIColor.redColor;

  [self.view addSubview:fakeImageView];
  [self.view addSubview:fakeTextView];

  [NSLayoutConstraint activateConstraints:@[
    [fakeImageView.heightAnchor constraintEqualToConstant:kFakeImageWidth],
    [fakeImageView.widthAnchor
        constraintEqualToAnchor:fakeImageView.heightAnchor],
    [fakeImageView.topAnchor constraintEqualToAnchor:self.view.topAnchor
                                            constant:kFakeSpacing],
    [fakeImageView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor
                       constant:kFakeImageLeadingSpacing],

    [fakeTextView.heightAnchor
        constraintEqualToAnchor:fakeImageView.heightAnchor],
    [fakeTextView.widthAnchor constraintEqualToConstant:kFakeTextBoxWidth],
    [fakeTextView.leadingAnchor
        constraintEqualToAnchor:fakeImageView.trailingAnchor
                       constant:kFakeImageToTextSpacing],
    [fakeTextView.topAnchor constraintEqualToAnchor:fakeImageView.topAnchor],
  ]];

  LayoutGuideCenter* layoutGuideCenter =
      self.popupViewController.layoutGuideCenter;
  [layoutGuideCenter referenceView:fakeImageView
                         underName:kOmniboxLeadingImageGuide];
  [layoutGuideCenter referenceView:fakeTextView
                         underName:kOmniboxTextFieldGuide];

  // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
  // used to get the style.
  ToolbarConfiguration* configuration =
      [[ToolbarConfiguration alloc] initWithStyle:ToolbarStyle::kNormal];

  UIView* containerView = [[UIView alloc] init];
  [containerView addSubview:self.popupViewController.view];
  containerView.backgroundColor = [configuration backgroundColor];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.popupViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.popupViewController.view, containerView);

  self.view.backgroundColor = UIColor.whiteColor;

  [self addChildViewController:self.popupViewController];
  [self.view addSubview:containerView];
  [self.popupViewController didMoveToParentViewController:self];
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor
        constraintEqualToAnchor:containerView.leadingAnchor],
    [self.view.trailingAnchor
        constraintEqualToAnchor:containerView.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:containerView.bottomAnchor],
    [containerView.topAnchor constraintEqualToAnchor:fakeImageView.bottomAnchor
                                            constant:kFakeSpacing],
  ]];
}

@end
