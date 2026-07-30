// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_bottom_sheet_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ntp/ui_bundled/fake_location_bar_view.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Snapping states for the bottom sheet.
typedef NS_ENUM(NSInteger, BottomSheetSnappingState) {
  BottomSheetSnappingStateCollapsed,
  BottomSheetSnappingStateExpanded,
};

// Minimum velocity needed for a user drag to trigger bottom sheet state change.
constexpr CGFloat kMinimumDragVelocityToChangeState = 500;
}  // namespace

@interface NewTabPageBottomSheetViewController () <UIGestureRecognizerDelegate>
@end

@implementation NewTabPageBottomSheetViewController {
  UIView* _dragHandle;
  FakeLocationBarView* _fakeLocationBar;

  NSLayoutConstraint* _bottomSheetTopConstraint;
  BottomSheetSnappingState _sheetState;

  CGSize _lastSize;
  CGFloat _initialConstant;
}

- (void)loadView {
  UIBlurEffect* blurEffect =
      [UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial];
  self.view = [[UIVisualEffectView alloc] initWithEffect:blurEffect];
}

- (void)viewDidLoad {
  [super viewDidLoad];

  _sheetState = BottomSheetSnappingStateCollapsed;

  self.view.layer.cornerRadius = 24.0;
  self.view.layer.masksToBounds = YES;

  UIVisualEffectView* visualEffectView = (UIVisualEffectView*)self.view;

  // Add drag handle to bottom sheet.
  _dragHandle = [[UIView alloc] init];
  _dragHandle.translatesAutoresizingMaskIntoConstraints = NO;
  _dragHandle.backgroundColor = [UIColor colorWithWhite:0.5 alpha:0.3];
  _dragHandle.layer.cornerRadius = 2.5;
  [visualEffectView.contentView addSubview:_dragHandle];

  [NSLayoutConstraint activateConstraints:@[
    [_dragHandle.centerXAnchor
        constraintEqualToAnchor:visualEffectView.contentView.centerXAnchor],
    [_dragHandle.topAnchor
        constraintEqualToAnchor:visualEffectView.contentView.topAnchor
                       constant:8],
    [_dragHandle.widthAnchor constraintEqualToConstant:36],
    [_dragHandle.heightAnchor constraintEqualToConstant:5],
  ]];

  // Add fake location bar.
  _fakeLocationBar = [[FakeLocationBarView alloc] init];
  _fakeLocationBar.translatesAutoresizingMaskIntoConstraints = NO;
  [_fakeLocationBar addTarget:self
                       action:@selector(fakeLocationBarTapped)
             forControlEvents:UIControlEventTouchUpInside];
  _fakeLocationBar.isAccessibilityElement = YES;
  _fakeLocationBar.accessibilityIdentifier = @"ntp-redesign-fake-omnibox";
  [visualEffectView.contentView addSubview:_fakeLocationBar];

  CGFloat fakeLocationBarHeight = 56.0;
  [NSLayoutConstraint activateConstraints:@[
    [_fakeLocationBar.topAnchor constraintEqualToAnchor:_dragHandle.bottomAnchor
                                               constant:16],
    [_fakeLocationBar.leadingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.leadingAnchor
                       constant:16],
    [_fakeLocationBar.trailingAnchor
        constraintEqualToAnchor:visualEffectView.contentView.trailingAnchor
                       constant:-16],
    [_fakeLocationBar.heightAnchor
        constraintEqualToConstant:fakeLocationBarHeight],
  ]];
  _fakeLocationBar.layer.cornerRadius = fakeLocationBarHeight / 2.0;

  // Add search icon and placeholder text inside the fake location bar.
  UIImage* searchIconImage =
      DefaultSymbolTemplateWithPointSize(kMagnifyingglassSymbol, 18);
  UIImageView* searchIcon = [[UIImageView alloc] initWithImage:searchIconImage];
  searchIcon.translatesAutoresizingMaskIntoConstraints = NO;
  searchIcon.tintColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  [_fakeLocationBar addSubview:searchIcon];

  UILabel* hintLabel = [[UILabel alloc] init];
  hintLabel.translatesAutoresizingMaskIntoConstraints = NO;
  hintLabel.textColor = [UIColor colorNamed:kTextfieldPlaceholderColor];
  hintLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  [_fakeLocationBar addSubview:hintLabel];

  [NSLayoutConstraint activateConstraints:@[
    [searchIcon.leadingAnchor
        constraintEqualToAnchor:_fakeLocationBar.leadingAnchor
                       constant:16],
    [searchIcon.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
    [searchIcon.widthAnchor constraintEqualToConstant:18],
    [searchIcon.heightAnchor constraintEqualToConstant:18],

    [hintLabel.leadingAnchor constraintEqualToAnchor:searchIcon.trailingAnchor
                                            constant:8],
    [hintLabel.trailingAnchor
        constraintEqualToAnchor:_fakeLocationBar.trailingAnchor
                       constant:-16],
    [hintLabel.centerYAnchor
        constraintEqualToAnchor:_fakeLocationBar.centerYAnchor],
  ]];

  // Set initial accessibility label for fake location bar.
  NSString* askGoogleString = l10n_util::GetNSStringF(
      IDS_OMNIBOX_EMPTY_ASK_HINT_WITH_DSE_NAME, std::u16string(u"Google"));
  _fakeLocationBar.accessibilityLabel = askGoogleString;
  hintLabel.text = askGoogleString;

  [_fakeLocationBar applyBackgroundTheme];
  [_fakeLocationBar updateColorsWithProgress:0.0 colorPalette:nil];

  // Add pan gesture recognizer.
  UIPanGestureRecognizer* panGesture =
      [[UIPanGestureRecognizer alloc] initWithTarget:self
                                              action:@selector(handlePan:)];
  panGesture.delegate = self;
  [self.view addGestureRecognizer:panGesture];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  [super didMoveToParentViewController:parent];
  if (parent) {
    [self setupSuperviewConstraints];
  }
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];
  if (self.view.superview &&
      !CGSizeEqualToSize(_lastSize, self.view.superview.bounds.size)) {
    _lastSize = self.view.superview.bounds.size;
    [self updateBottomSheetPositionAnimated:NO];
  }
}

- (void)invalidate {
  self.delegate = nil;
}

#pragma mark - Action Targets

- (void)fakeLocationBarTapped {
  [self.delegate bottomSheetViewControllerDidTapFakeLocationBar:self];
}

#pragma mark - Snapping Offsets

- (CGFloat)collapsedOffset {
  UIView* superview = self.view.superview;
  return superview ? superview.bounds.size.height * 0.55 : 0;
}

- (CGFloat)expandedOffset {
  UIView* superview = self.view.superview;
  return superview ? 20.0 + superview.safeAreaInsets.top : 0;
}

#pragma mark - Bottom Sheet Snapping and Panning

- (void)setupSuperviewConstraints {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [self.view.leadingAnchor constraintEqualToAnchor:superview.leadingAnchor],
    [self.view.trailingAnchor constraintEqualToAnchor:superview.trailingAnchor],
    [self.view.bottomAnchor constraintEqualToAnchor:superview.bottomAnchor],
  ]];

  if (!_bottomSheetTopConstraint) {
    _bottomSheetTopConstraint =
        [self.view.topAnchor constraintEqualToAnchor:superview.topAnchor
                                            constant:[self collapsedOffset]];
    _bottomSheetTopConstraint.active = YES;
  }
}

- (void)updateBottomSheetPositionAnimated:(BOOL)animated {
  if (!_bottomSheetTopConstraint) {
    return;
  }
  CGFloat targetConstant = (_sheetState == BottomSheetSnappingStateCollapsed)
                               ? [self collapsedOffset]
                               : [self expandedOffset];

  if (!animated) {
    _bottomSheetTopConstraint.constant = targetConstant;
  } else {
    __weak __typeof(self) weakSelf = self;
    [UIView animateWithDuration:0.3
                          delay:0
         usingSpringWithDamping:0.8
          initialSpringVelocity:0.5
                        options:UIViewAnimationOptionCurveEaseInOut
                     animations:^{
                       NewTabPageBottomSheetViewController* strongSelf =
                           weakSelf;
                       if (!strongSelf) {
                         return;
                       }
                       strongSelf->_bottomSheetTopConstraint.constant =
                           targetConstant;
                       [strongSelf.view.superview layoutIfNeeded];
                     }
                     completion:nil];
  }
}

- (void)handlePan:(UIPanGestureRecognizer*)gesture {
  UIView* superview = self.view.superview;
  if (!superview) {
    return;
  }
  CGPoint translation = [gesture translationInView:superview];
  CGPoint velocity = [gesture velocityInView:superview];

  if (gesture.state == UIGestureRecognizerStateBegan) {
    _initialConstant = _bottomSheetTopConstraint.constant;
  }

  CGFloat newConstant = _initialConstant + translation.y;
  CGFloat minOffset = [self expandedOffset];
  CGFloat maxOffset = [self collapsedOffset];

  if (newConstant < minOffset) {
    newConstant = minOffset;
  } else if (newConstant > maxOffset) {
    newConstant = maxOffset;
  }

  _bottomSheetTopConstraint.constant = newConstant;

  if (gesture.state == UIGestureRecognizerStateEnded) {
    CGFloat collapsedOffset = [self collapsedOffset];
    CGFloat expandedOffset = [self expandedOffset];
    CGFloat midPoint = (collapsedOffset + expandedOffset) / 2.0;

    BottomSheetSnappingState targetState;
    if (velocity.y > kMinimumDragVelocityToChangeState) {
      targetState = BottomSheetSnappingStateCollapsed;
    } else if (velocity.y < -kMinimumDragVelocityToChangeState) {
      targetState = BottomSheetSnappingStateExpanded;
    } else {
      if (newConstant > midPoint) {
        targetState = BottomSheetSnappingStateCollapsed;
      } else {
        targetState = BottomSheetSnappingStateExpanded;
      }
    }

    _sheetState = targetState;
    [self updateBottomSheetPositionAnimated:YES];
  }
}

@end
