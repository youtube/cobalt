// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_commands.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_constants.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_empty_state_view_controller.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/shared/ui/elements/extended_touch_target_button.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
// Constants for layout.
constexpr CGFloat kSearchFieldMargin = 16.0;
constexpr CGFloat kHeaderBarHeight = 56.0;
constexpr CGFloat kCloseButtonMargin = 16.0;
}  // namespace

@interface AtMemoryViewController () <UISearchBarDelegate>
@end

@implementation AtMemoryViewController {
  UISearchBar* _searchBar;
  UIView* _headerBar;
  UIViewController* _childViewController;
  UIView* _containerView;
  AtMemoryEmptyStateViewController* _emptyStateViewController;
  at_memory::AtMemoryContentState _contentState;
}

- (instancetype)init {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    // Custom initialization.
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  [self setupHeaderBar];
  [self setupSearchBar];
  [self setupContainerView];
  [self setupLayoutConstraints];

  [self applyContentState];
}

- (void)setupContainerView {
  _containerView = [[UIView alloc] init];
  _containerView.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_containerView];
}

- (void)setChildViewController:(UIViewController*)childViewController {
  if (_childViewController == childViewController) {
    return;
  }

  if (_childViewController) {
    [_childViewController willMoveToParentViewController:nil];
    [_childViewController.view removeFromSuperview];
    [_childViewController removeFromParentViewController];
  }

  _childViewController = childViewController;

  if (!_childViewController) {
    return;
  }

  _childViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [self addChildViewController:_childViewController];
  [_containerView addSubview:_childViewController.view];
  [_childViewController didMoveToParentViewController:self];

  AddSameConstraints(_containerView, _childViewController.view);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [_searchBar.searchTextField becomeFirstResponder];
}

#pragma mark - UI Setup

- (void)setupHeaderBar {
  _headerBar = [[UIView alloc] init];
  _headerBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_headerBar];

  UIButton* closeButton = [self createCloseButton];
  [_headerBar addSubview:closeButton];

  [NSLayoutConstraint activateConstraints:@[
    [closeButton.centerYAnchor
        constraintEqualToAnchor:_headerBar.centerYAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:_headerBar.trailingAnchor
                       constant:-kCloseButtonMargin],
  ]];
}

- (UIButton*)createCloseButton {
  ExtendedTouchTargetButton* closeButton =
      [ExtendedTouchTargetButton buttonWithType:UIButtonTypeSystem];
  closeButton.accessibilityIdentifier =
      kAtMemoryCloseButtonAccessibilityIdentifier;
  closeButton.accessibilityLabel = l10n_util::GetNSString(IDS_CLOSE);
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;

  UIImageSymbolConfiguration* symbolConfiguration =
      at_memory::GetCloseButtonSymbolConfiguration();
  UIImage* buttonImage =
      SymbolWithPalette(DefaultSymbolWithConfiguration(kXMarkCircleFillSymbol,
                                                       symbolConfiguration),
                        @[
                          at_memory::GetCloseButtonForegroundColor(),
                          [UIColor tertiarySystemFillColor]
                        ]);
  [closeButton setImage:buttonImage forState:UIControlStateNormal];

  [closeButton addTarget:self
                  action:@selector(didTapClose)
        forControlEvents:UIControlEventTouchUpInside];
  return closeButton;
}

- (void)setupSearchBar {
  _searchBar = [[UISearchBar alloc] init];
  _searchBar.backgroundImage = [[UIImage alloc] init];
  _searchBar.searchTextField.accessibilityIdentifier =
      kAtMemorySearchBarAccessibilityIdentifier;
  _searchBar.delegate = self;
  _searchBar.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:_searchBar];
}

- (void)setupLayoutConstraints {
  [NSLayoutConstraint activateConstraints:@[
    // Header constraints
    [_headerBar.topAnchor
        constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor],
    [_headerBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
    [_headerBar.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_headerBar.heightAnchor constraintEqualToConstant:kHeaderBarHeight],

    // Container View constraints
    [_containerView.topAnchor constraintEqualToAnchor:_headerBar.bottomAnchor],
    [_containerView.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [_containerView.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [_containerView.bottomAnchor constraintEqualToAnchor:_searchBar.topAnchor
                                                constant:-kSearchFieldMargin],

    // Search Bar constraints
    [_searchBar.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor
                                             constant:kSearchFieldMargin],
    [_searchBar.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor
                                              constant:-kSearchFieldMargin],
    [_searchBar.bottomAnchor
        constraintEqualToAnchor:self.view.keyboardLayoutGuide.topAnchor
                       constant:-kSearchFieldMargin],
  ]];
}

- (void)didTapClose {
  [self.atMemoryHandler dismissAtMemory];
}

#pragma mark - UISearchBarDelegate

- (void)searchBar:(UISearchBar*)searchBar textDidChange:(NSString*)searchText {
  // TODO(crbug.com/522338028): Handle other states.
  // Handle search updates.
}

#pragma mark - AtMemoryConsumer

- (void)setContentState:(at_memory::AtMemoryContentState)contentState {
  _contentState = contentState;
  if (!self.isViewLoaded) {
    return;
  }
  [self applyContentState];
}

- (void)applyContentState {
  if (_contentState == at_memory::AtMemoryContentState::kEmpty) {
    if (!_emptyStateViewController) {
      _emptyStateViewController =
          [[AtMemoryEmptyStateViewController alloc] init];
    }
    [self setChildViewController:_emptyStateViewController];
  } else {
    // TODO(crbug.com/522326512): Handle other states.
    [self setChildViewController:nil];
  }
}

@end
