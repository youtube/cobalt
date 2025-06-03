// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/promo_style/promo_style_view_controller.h"

#import "base/check.h"
#import "base/check_op.h"
#import "base/i18n/rtl.h"
#import "base/notreached.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/constants.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/highlight_button.h"
#import "ios/chrome/common/ui/promo_style/constants.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/common/ui/util/device_util.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/common/ui/util/pointer_interaction_util.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ios/chrome/common/ui/util/ui_util.h"

namespace {

constexpr CGFloat kDefaultMargin = 16;
// Default margin between the subtitle and the content view.
constexpr CGFloat kDefaultSubtitleBottomMargin = 22;
// Top margin for no background header image in percentage of the dialog size.
constexpr CGFloat kNoBackgroundHeaderImageTopMarginPercentage = 0.04;
constexpr CGFloat kNoBackgroundHeaderImageBottomMargin = 5;
// Top margin for header image with background in percentage of the dialog size.
constexpr CGFloat kHeaderImageBackgroundTopMarginPercentage = 0.1;
constexpr CGFloat kHeaderImageBackgroundBottomMargin = 34;
constexpr CGFloat kTitleHorizontalMargin = 18;
constexpr CGFloat kActionsBottomMargin = 10;
constexpr CGFloat kTallBannerMultiplier = 0.35;
constexpr CGFloat kExtraTallBannerMultiplier = 0.5;
constexpr CGFloat kDefaultBannerMultiplier = 0.25;
constexpr CGFloat kContentWidthMultiplier = 0.8;
constexpr CGFloat kContentOptimalWidth = 327;
constexpr CGFloat kMoreArrowMargin = 4;
constexpr CGFloat kPreviousContentVisibleOnScroll = 0.15;
constexpr CGFloat kSeparatorHeight = 1;
constexpr CGFloat kLearnMoreButtonSide = 40;
constexpr CGFloat kheaderImageSize = 48;
constexpr CGFloat kFullheaderImageSize = 100;

// Corner radius for the whole view.
constexpr CGFloat kCornerRadius = 20;

// Properties of the header image kImageWithShadow type shadow.
const CGFloat kHeaderImageCornerRadius = 13;
const CGFloat kHeaderImageShadowOffsetX = 0;
const CGFloat kHeaderImageShadowOffsetY = 0;
const CGFloat kHeaderImageShadowRadius = 6;
const CGFloat kHeaderImageShadowOpacity = 0.1;
const CGFloat kHeaderImageShadowShadowInset = 20;

}  // namespace

@interface PromoStyleViewController () <UIScrollViewDelegate>

@property(nonatomic, strong) UIImageView* bannerImageView;
// This view contains only the header image.
@property(nonatomic, strong) UIImageView* headerImageView;
@property(nonatomic, strong) UITextView* disclaimerView;
@property(nonatomic, strong) HighlightButton* primaryActionButton;

// Read/Write override.
@property(nonatomic, assign, readwrite) BOOL didReachBottom;

@end

@implementation PromoStyleViewController {
  // Whether banner is light or dark mode
  UIUserInterfaceStyle _bannerStyle;

  UIScrollView* _scrollView;
  // UIView that wraps the scrollable content.
  UIView* _scrollContentView;
  // This view contains the header image with a shadow background image behind.
  UIView* _fullHeaderImageView;
  // This view contains the background image for the header image. The header
  // view will be placed at the center of it.
  UIImageView* _headerBackgroundImageView;
  UILabel* _subtitleLabel;
  UIStackView* _actionStackView;
  UIButton* _secondaryActionButton;
  UIButton* _tertiaryActionButton;

  UIView* _separator;
  CGFloat _scrollViewBottomOffsetY;

  // Layout constraint for `headerBackgroundImageView` top margin.
  NSLayoutConstraint* _headerBackgroundImageViewTopMargin;

  // YES if the views can be updated on scroll updates (e.g., change the text
  // label string of the primary button) which corresponds to the moment where
  // the layout reflects the latest updates.
  BOOL _canUpdateViewsOnScroll;

  // Whether the image is currently being calculated; used to prevent infinite
  // recursions caused by `viewDidLayoutSubviews`.
  BOOL _calculatingImageSize;

  // Vertical constraints for buttons; used to reset top anchors when the number
  // of buttons changes on scroll.
  NSArray<NSLayoutConstraint*>* _buttonsVerticalAnchorConstraints;

  // Vertical constraints for banner; used to deactivate these constraints when
  // the banner is hidden.
  NSArray<NSLayoutConstraint*>* _bannerConstraints;

  // Indicate that the view should scroll to the bottom at the end of the next
  // layout.
  BOOL _shouldScrollToBottom;

  // Wheter the buttons have been updated from "More" to the action buttons.
  BOOL _buttonUpdated;
}

@synthesize learnMoreButton = _learnMoreButton;

#pragma mark - Public

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil {
  self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
  if (self) {
    _titleHorizontalMargin = kTitleHorizontalMargin;
    _subtitleBottomMargin = kDefaultSubtitleBottomMargin;
  }

  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UIView* view = self.view;
  view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];

  // Create a layout guide for the margin between the subtitle and the screen-
  // specific content. A layout guide is needed because the margin scales with
  // the view height.
  UILayoutGuide* subtitleMarginLayoutGuide = [[UILayoutGuide alloc] init];

  _separator = [[UIView alloc] init];
  _bannerStyle = UIUserInterfaceStyleUnspecified;
  _separator.translatesAutoresizingMaskIntoConstraints = NO;
  _separator.backgroundColor = [UIColor colorNamed:kSeparatorColor];
  _separator.hidden = YES;
  [view addSubview:_separator];

  _scrollContentView = [[UIView alloc] init];
  _scrollContentView.translatesAutoresizingMaskIntoConstraints = NO;
  [_scrollContentView addSubview:self.bannerImageView];
  if (self.headerImageType != PromoStyleImageType::kNone) {
    _fullHeaderImageView = [self createFullHeaderImageView];
    _headerBackgroundImageView = [self createheaderBackgroundImageView];
    [_scrollContentView addSubview:_headerBackgroundImageView];
    [_headerBackgroundImageView addSubview:_fullHeaderImageView];
    [_fullHeaderImageView addSubview:self.headerImageView];
  }

  UILabel* titleLabel = self.titleLabel;
  [_scrollContentView addSubview:titleLabel];
  _subtitleLabel = [self createSubtitleLabel];
  [_scrollContentView addSubview:_subtitleLabel];
  [view addLayoutGuide:subtitleMarginLayoutGuide];

  UIView* specificContentView = self.specificContentView;
  [_scrollContentView addSubview:specificContentView];

  UITextView* disclaimerView = self.disclaimerView;
  if (disclaimerView) {
    [_scrollContentView addSubview:disclaimerView];
  }

  // Wrap everything except the action buttons in a scroll view, to support
  // dynamic types.
  _scrollView = [self createScrollView];
  [_scrollView addSubview:_scrollContentView];
  [view addSubview:_scrollView];

  // Add learn more button to top left of the view, if requested
  if (self.shouldShowLearnMoreButton) {
    [view insertSubview:self.learnMoreButton aboveSubview:_scrollView];
  }

  _actionStackView = [[UIStackView alloc] init];
  _actionStackView.alignment = UIStackViewAlignmentFill;
  _actionStackView.axis = UILayoutConstraintAxisVertical;
  _actionStackView.translatesAutoresizingMaskIntoConstraints = NO;
  [_actionStackView addArrangedSubview:self.primaryActionButton];
  [view addSubview:_actionStackView];

  // Create a layout guide to constrain the width of the content, while still
  // allowing the scroll view to take the full screen width.
  UILayoutGuide* widthLayoutGuide = [[UILayoutGuide alloc] init];
  [view addLayoutGuide:widthLayoutGuide];

  if (disclaimerView) {
    [NSLayoutConstraint activateConstraints:@[
      [disclaimerView.topAnchor
          constraintEqualToAnchor:specificContentView.bottomAnchor
                         constant:kDefaultMargin],
      [disclaimerView.leadingAnchor
          constraintEqualToAnchor:_scrollContentView.leadingAnchor],
      [disclaimerView.trailingAnchor
          constraintEqualToAnchor:_scrollContentView.trailingAnchor],

    ]];
    if (self.topAlignedLayout) {
      [NSLayoutConstraint activateConstraints:@[
        [disclaimerView.bottomAnchor
            constraintLessThanOrEqualToAnchor:_scrollContentView.bottomAnchor]
      ]];
    } else {
      [NSLayoutConstraint activateConstraints:@[
        [disclaimerView.bottomAnchor
            constraintEqualToAnchor:_scrollContentView.bottomAnchor]
      ]];
    }
  } else {
    [_scrollContentView.bottomAnchor
        constraintEqualToAnchor:specificContentView.bottomAnchor]
        .active = YES;
  }

  [NSLayoutConstraint activateConstraints:@[
    // Content width layout guide constraints. Constrain the width to both at
    // least 65% of the view width, and to the full view width with margins.
    // This is to accomodate the iPad layout, which cannot be isolated out using
    // the traitCollection because of the FormSheet presentation style
    // (iPad FormSheet is considered compact).
    [widthLayoutGuide.centerXAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.centerXAnchor],
    [widthLayoutGuide.widthAnchor
        constraintGreaterThanOrEqualToAnchor:view.safeAreaLayoutGuide
                                                 .widthAnchor
                                  multiplier:kContentWidthMultiplier],
    [widthLayoutGuide.widthAnchor
        constraintLessThanOrEqualToAnchor:view.safeAreaLayoutGuide.widthAnchor
                                 constant:-2 * kDefaultMargin],

    // Scroll view constraints.
    [_scrollView.topAnchor
        constraintEqualToAnchor:view.safeAreaLayoutGuide.topAnchor],
    [_scrollView.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_scrollView.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],

    // Separator constraints.
    [_separator.heightAnchor constraintEqualToConstant:kSeparatorHeight],
    [_separator.leadingAnchor constraintEqualToAnchor:view.leadingAnchor],
    [_separator.trailingAnchor constraintEqualToAnchor:view.trailingAnchor],
    [_separator.topAnchor constraintEqualToAnchor:_scrollView.bottomAnchor],

    // Scroll content view constraints. Constrain its height to at least the
    // scroll view height, so that derived VCs can pin UI elements just above
    // the buttons.
    [_scrollContentView.topAnchor
        constraintEqualToAnchor:_scrollView.topAnchor],
    [_scrollContentView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [_scrollContentView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
    [_scrollContentView.bottomAnchor
        constraintEqualToAnchor:_scrollView.bottomAnchor],
    [_scrollContentView.heightAnchor
        constraintGreaterThanOrEqualToAnchor:_scrollView.heightAnchor],

    // Labels contraints. Attach them to the top of the scroll content view, and
    // center them horizontally.
    [titleLabel.centerXAnchor
        constraintEqualToAnchor:_scrollContentView.centerXAnchor],
    [titleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:_scrollContentView.widthAnchor
                                 constant:-2 * self.titleHorizontalMargin],
    [_subtitleLabel.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                             constant:kDefaultMargin],
    [_subtitleLabel.centerXAnchor
        constraintEqualToAnchor:_scrollContentView.centerXAnchor],
    [_subtitleLabel.widthAnchor
        constraintLessThanOrEqualToAnchor:_scrollContentView.widthAnchor],

    // Constraints for the screen-specific content view. It should take the
    // remaining scroll view area, with some margins on the top and sides.
    [subtitleMarginLayoutGuide.topAnchor
        constraintEqualToAnchor:_subtitleLabel.bottomAnchor],
    [subtitleMarginLayoutGuide.heightAnchor
        constraintEqualToConstant:_subtitleBottomMargin],

    [specificContentView.leadingAnchor
        constraintEqualToAnchor:_scrollContentView.leadingAnchor],
    [specificContentView.trailingAnchor
        constraintEqualToAnchor:_scrollContentView.trailingAnchor],

    // Action stack view constraints. Constrain the bottom of the action stack
    // view to both the bottom of the screen and the bottom of the safe area, to
    // give a nice result whether the device has a physical home button or not.
    [_actionStackView.leadingAnchor
        constraintEqualToAnchor:widthLayoutGuide.leadingAnchor],
    [_actionStackView.trailingAnchor
        constraintEqualToAnchor:widthLayoutGuide.trailingAnchor],
  ]];

  if (self.hideSpecificContentView) {
    // Hide the specificContentView and do not add the margin.
    [NSLayoutConstraint activateConstraints:@[
      [specificContentView.topAnchor
          constraintEqualToAnchor:_subtitleLabel.bottomAnchor],
      [specificContentView.heightAnchor constraintEqualToConstant:0]
    ]];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [specificContentView.topAnchor
          constraintEqualToAnchor:subtitleMarginLayoutGuide.bottomAnchor]
    ]];
  }

  if (self.bannerLimitWithRoundedCorner) {
    // Add a subview with the same background of the view and put it over the
    // banner image view.
    UIView* limitView = [[UIView alloc] init];
    limitView.clipsToBounds = YES;
    limitView.translatesAutoresizingMaskIntoConstraints = NO;
    limitView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
    [_scrollContentView insertSubview:limitView
                         aboveSubview:self.bannerImageView];

    // Corner radius cannot reach over half of the view, so set the height to 2*
    // kCornerRadius.
    [NSLayoutConstraint activateConstraints:@[
      [limitView.centerYAnchor
          constraintEqualToAnchor:_bannerImageView.bottomAnchor],
      [limitView.leftAnchor constraintEqualToAnchor:self.view.leftAnchor],
      [limitView.rightAnchor constraintEqualToAnchor:self.view.rightAnchor],
      [limitView.heightAnchor constraintEqualToConstant:2 * kCornerRadius],
    ]];
    limitView.layer.cornerRadius = kCornerRadius;
    limitView.layer.maskedCorners =
        kCALayerMaxXMinYCorner | kCALayerMinXMinYCorner;
    limitView.layer.masksToBounds = true;
  }

  if (self.headerImageType != PromoStyleImageType::kNone) {
    _headerBackgroundImageViewTopMargin = [_headerBackgroundImageView.topAnchor
        constraintEqualToAnchor:self.bannerImageView.bottomAnchor];
    CGFloat headerImageBottomMargin = kDefaultMargin;
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      headerImageBottomMargin = self.headerBackgroundImage == nil
                                    ? kNoBackgroundHeaderImageBottomMargin
                                    : kHeaderImageBackgroundBottomMargin;
    }
    UIImageView* headerImageView = self.headerImageView;
    [NSLayoutConstraint activateConstraints:@[
      _headerBackgroundImageViewTopMargin,
      [titleLabel.topAnchor
          constraintEqualToAnchor:_headerBackgroundImageView.bottomAnchor
                         constant:headerImageBottomMargin],
      [_headerBackgroundImageView.centerXAnchor
          constraintEqualToAnchor:_scrollContentView.centerXAnchor],
      [_headerBackgroundImageView.centerXAnchor
          constraintEqualToAnchor:_fullHeaderImageView.centerXAnchor],
      [_headerBackgroundImageView.centerYAnchor
          constraintEqualToAnchor:_fullHeaderImageView.centerYAnchor],
      [_fullHeaderImageView.centerXAnchor
          constraintEqualToAnchor:headerImageView.centerXAnchor],
      [_fullHeaderImageView.centerYAnchor
          constraintEqualToAnchor:headerImageView.centerYAnchor],
    ]];
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      [NSLayoutConstraint activateConstraints:@[
        [_fullHeaderImageView.widthAnchor
            constraintEqualToConstant:kFullheaderImageSize],
        [_fullHeaderImageView.heightAnchor
            constraintEqualToConstant:kFullheaderImageSize],
        [_headerBackgroundImageView.widthAnchor
            constraintGreaterThanOrEqualToConstant:kFullheaderImageSize],
        [_headerBackgroundImageView.heightAnchor
            constraintGreaterThanOrEqualToConstant:kFullheaderImageSize],
        [headerImageView.widthAnchor
            constraintEqualToConstant:kheaderImageSize],
        [headerImageView.heightAnchor
            constraintEqualToConstant:kheaderImageSize],
      ]];
    }
    if (self.headerImageType == PromoStyleImageType::kImage ||
        self.headerImageType == PromoStyleImageType::kImageWithShadow) {
      [NSLayoutConstraint activateConstraints:@[
        [_fullHeaderImageView.widthAnchor
            constraintEqualToAnchor:_fullHeaderImageView.heightAnchor],
        [_fullHeaderImageView.widthAnchor
            constraintGreaterThanOrEqualToAnchor:headerImageView.widthAnchor
                                        constant:kHeaderImageShadowShadowInset],
        [_fullHeaderImageView.heightAnchor
            constraintGreaterThanOrEqualToAnchor:headerImageView.heightAnchor
                                        constant:kHeaderImageShadowShadowInset],

        [_headerBackgroundImageView.widthAnchor
            constraintGreaterThanOrEqualToAnchor:_fullHeaderImageView
                                                     .widthAnchor],
        [_headerBackgroundImageView.heightAnchor
            constraintGreaterThanOrEqualToAnchor:_fullHeaderImageView
                                                     .heightAnchor],
      ]];
      // Set low priority constraint to set the width/height according to image
      // size. If image ratio is not 1:1, this will conflict with the
      // height = width constraint abve and one of the 2 will be dropped.
      NSLayoutConstraint* widthConstraint = [_fullHeaderImageView.widthAnchor
          constraintEqualToAnchor:headerImageView.widthAnchor
                         constant:kHeaderImageShadowShadowInset];
      widthConstraint.priority = UILayoutPriorityDefaultLow;
      widthConstraint.active = YES;
      NSLayoutConstraint* heightConstraint = [_fullHeaderImageView.heightAnchor
          constraintEqualToAnchor:headerImageView.heightAnchor
                         constant:kHeaderImageShadowShadowInset];
      heightConstraint.priority = UILayoutPriorityDefaultLow;
      heightConstraint.active = YES;
    }
    [_headerBackgroundImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];
    [_headerBackgroundImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisVertical];
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [titleLabel.topAnchor
          constraintEqualToAnchor:self.bannerImageView.bottomAnchor],
    ]];
  }

  [self setupBannerConstraints];

  _buttonsVerticalAnchorConstraints = @[
    [_scrollView.bottomAnchor constraintEqualToAnchor:_actionStackView.topAnchor
                                             constant:-kDefaultMargin],
    [_actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:view.bottomAnchor
                                 constant:-kActionsBottomMargin * 2],
    [_actionStackView.bottomAnchor
        constraintLessThanOrEqualToAnchor:view.safeAreaLayoutGuide.bottomAnchor
                                 constant:-kActionsBottomMargin],
  ];
  [NSLayoutConstraint activateConstraints:_buttonsVerticalAnchorConstraints];

  // This constraint is added to enforce that the content width should be as
  // close to the optimal width as possible, within the range already activated
  // for "widthLayoutGuide.widthAnchor" previously, with a higher priority.
  // In this case, the content width in iPad and iPhone landscape mode should be
  // the safe layout width multiplied by kContentWidthMultiplier, while the
  // content width for a iPhone portrait mode should be kContentOptimalWidth.
  NSLayoutConstraint* contentLayoutGuideWidthConstraint =
      [widthLayoutGuide.widthAnchor
          constraintEqualToConstant:kContentOptimalWidth];
  contentLayoutGuideWidthConstraint.priority = UILayoutPriorityRequired - 1;
  contentLayoutGuideWidthConstraint.active = YES;

  // Also constrain the bottom of the action stack view to the bottom of the
  // safe area, but with a lower priority, so that the action stack view is put
  // as close to the bottom as possible.
  NSLayoutConstraint* actionBottomConstraint = [_actionStackView.bottomAnchor
      constraintEqualToAnchor:view.safeAreaLayoutGuide.bottomAnchor];
  actionBottomConstraint.priority = UILayoutPriorityDefaultLow;
  actionBottomConstraint.active = YES;

  if (self.shouldShowLearnMoreButton) {
    UIButton* learnMoreButton = self.learnMoreButton;
    [NSLayoutConstraint activateConstraints:@[
      [learnMoreButton.topAnchor
          constraintEqualToAnchor:_scrollContentView.topAnchor],
      [learnMoreButton.leadingAnchor
          constraintEqualToAnchor:view.safeAreaLayoutGuide.leadingAnchor],
      [learnMoreButton.widthAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
      [learnMoreButton.heightAnchor
          constraintEqualToConstant:kLearnMoreButtonSide],
    ]];
  }
}

- (void)viewWillDisappear:(BOOL)animated {
  _canUpdateViewsOnScroll = NO;
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Set `didReachBottom` to YES when `scrollToEndMandatory` is NO, since the
  // screen can already be considered as fully scrolled when scrolling to the
  // end isn't mandatory.
  if (!self.scrollToEndMandatory) {
    self.didReachBottom = YES;
  }

  // Only add the scroll view delegate after all the view layouts are fully
  // done.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self setupScrollView];
  });
}

- (void)viewDidLayoutSubviews {
  [super viewDidLayoutSubviews];

  // Prevents potential recursive calls to `viewDidLayoutSubviews`.
  if (_calculatingImageSize) {
    return;
  }
  // Rescale image here as on iPad the view height isn't correctly set before
  // subviews are laid out.
  _calculatingImageSize = YES;
  self.bannerImageView.image =
      [self scaleBannerWithCurrentImage:self.bannerImageView.image
                                 toSize:[self computeBannerImageSize]];
  _calculatingImageSize = NO;
  if (_shouldScrollToBottom) {
    _shouldScrollToBottom = NO;
    dispatch_async(dispatch_get_main_queue(), ^{
      [self scrollToBottom];
    });
  }
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  // Update the buttons once the layout changes take effect to have the right
  // measurements to evaluate the scroll position.
  void (^transition)(id<UIViewControllerTransitionCoordinatorContext>) =
      ^(id<UIViewControllerTransitionCoordinatorContext> context) {
        [self updateViewsOnScrollViewUpdate];
      };
  [coordinator animateAlongsideTransition:transition completion:nil];
}

- (void)viewWillLayoutSubviews {
  [super viewWillLayoutSubviews];
  if (!self.topAlignedLayout) {
    CGFloat headerImageTopMarginPercentage =
        self.headerBackgroundImage == nil
            ? kNoBackgroundHeaderImageTopMarginPercentage
            : kHeaderImageBackgroundTopMarginPercentage;
    _headerBackgroundImageViewTopMargin.constant = AlignValueToPixel(
        self.view.bounds.size.height * headerImageTopMarginPercentage);
  }
}

#pragma mark - UITraitEnvironment

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  // Reset the title font and the learn more text to make sure that they are
  // properly scaled. Nothing will be done for the Read More text if the
  // bottom is reached.
  [self setFontForTitle:self.titleLabel];
  [self setReadMoreText];

  // Update the primary button once the layout changes take effect to have the
  // right measurements to evaluate the scroll position.
  dispatch_async(dispatch_get_main_queue(), ^{
    [self updateViewsOnScrollViewUpdate];
  });
}

#pragma mark - Accessors

- (void)setShouldBannerFillTopSpace:(BOOL)shouldBannerFillTopSpace {
  _shouldBannerFillTopSpace = shouldBannerFillTopSpace;
  [self setupBannerConstraints];
  self.bannerImageView.image =
      [self scaleBannerWithCurrentImage:self.bannerImageView.image
                                 toSize:[self computeBannerImageSize]];
}

- (void)setShouldHideBanner:(BOOL)shouldHideBanner {
  _shouldHideBanner = shouldHideBanner;
  [self setupBannerConstraints];
  self.bannerImageView.image =
      [self scaleBannerWithCurrentImage:self.bannerImageView.image
                                 toSize:[self computeBannerImageSize]];
}

- (void)setPrimaryActionString:(NSString*)text {
  _primaryActionString = text;
  // Change the button's label, unless scrolling to the end is mandatory and the
  // scroll view hasn't been scrolled to the end at least once yet.
  if (_primaryActionButton &&
      (!self.scrollToEndMandatory || self.didReachBottom)) {
    [_primaryActionButton setAttributedTitle:nil forState:UIControlStateNormal];
    [_primaryActionButton setTitle:_primaryActionString
                          forState:UIControlStateNormal];
  }
}

- (UIImageView*)bannerImageView {
  if (!_bannerImageView) {
    _bannerImageView = [[UIImageView alloc]
        initWithImage:
            [self scaleBannerWithCurrentImage:nil
                                       toSize:[self computeBannerImageSize]]];
    _bannerImageView.clipsToBounds = YES;
    _bannerImageView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _bannerImageView;
}

- (UIImageView*)headerImageView {
  if (!_headerImageView) {
    DCHECK(self.headerImageType != PromoStyleImageType::kNone);
    _headerImageView = [[UIImageView alloc] initWithImage:self.headerImage];
    _headerImageView.clipsToBounds = YES;
    _headerImageView.translatesAutoresizingMaskIntoConstraints = NO;
    if (self.headerImageType == PromoStyleImageType::kAvatar) {
      _headerImageView.layer.cornerRadius = kheaderImageSize / 2.;
    }
    _headerImageView.image = _headerImage;
    _headerImageView.accessibilityLabel = _headerAccessibilityLabel;
    _headerImageView.isAccessibilityElement = _headerAccessibilityLabel != nil;
  }
  return _headerImageView;
}

- (void)setHeaderImage:(UIImage*)headerImage {
  _headerImage = headerImage;
  if (self.headerImageType == PromoStyleImageType::kAvatar) {
    DCHECK_EQ(headerImage.size.width, kheaderImageSize);
    DCHECK_EQ(headerImage.size.height, kheaderImageSize);
  }
  // `self.headerImageView` should not be used to avoid creating the image.
  // The owner might set the image first and then change the value of
  // `self.headerImageType`.
  _headerImageView.image = headerImage;
}

- (void)setHeaderAccessibilityLabel:(NSString*)headerAccessibilityLabel {
  _headerAccessibilityLabel = headerAccessibilityLabel;
  // `self.headerImageView` should not be used to avoid creating the image.
  // The owner might set the accessibility label and then change the value of
  // `self.headerImageType`.
  _headerImageView.accessibilityLabel = headerAccessibilityLabel;
  _headerImageView.isAccessibilityElement = headerAccessibilityLabel != nil;
}

- (UILabel*)titleLabel {
  if (!_titleLabel) {
    _titleLabel = [[UILabel alloc] init];
    _titleLabel.numberOfLines = 0;
    [self setFontForTitle:_titleLabel];
    _titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
    _titleLabel.text = self.titleText;
    _titleLabel.textAlignment = NSTextAlignmentCenter;
    _titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _titleLabel.adjustsFontForContentSizeCategory = YES;
    _titleLabel.accessibilityIdentifier =
        kPromoStyleTitleAccessibilityIdentifier;
    _titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  }
  return _titleLabel;
}

- (UIView*)specificContentView {
  if (!_specificContentView) {
    _specificContentView = [[UIView alloc] init];
    _specificContentView.translatesAutoresizingMaskIntoConstraints = NO;
  }
  return _specificContentView;
}

- (UIButton*)primaryActionButton {
  if (!_primaryActionButton) {
    _primaryActionButton = [[HighlightButton alloc] initWithFrame:CGRectZero];

    // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
    // deployment target is iOS 15.
    UIEdgeInsets contentInsets =
        UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
    SetContentEdgeInsets(_primaryActionButton, contentInsets);
    UIEdgeInsets titleInsets =
        UIEdgeInsetsMake(0, kMoreArrowMargin, 0, kMoreArrowMargin);
    SetTitleEdgeInsets(_primaryActionButton, titleInsets);

    [_primaryActionButton setBackgroundColor:[UIColor colorNamed:kBlueColor]];
    UIColor* titleColor = [UIColor colorNamed:kSolidButtonTextColor];
    [_primaryActionButton setTitleColor:titleColor
                               forState:UIControlStateNormal];
    [self setPrimaryActionButtonFont:_primaryActionButton];
    _primaryActionButton.layer.cornerRadius = kPrimaryButtonCornerRadius;
    _primaryActionButton.translatesAutoresizingMaskIntoConstraints = NO;

    _primaryActionButton.pointerInteractionEnabled = YES;
    _primaryActionButton.pointerStyleProvider =
        CreateOpaqueButtonPointerStyleProvider();

    // Use `primaryActionString` even if scrolling to the end is mandatory
    // because at the viewDidLoad stage, the scroll view hasn't computed its
    // content height, so there is no way to knOow if scrolling is needed.
    // This label will be updated at the viewDidAppear stage if necessary.
    [_primaryActionButton setTitle:self.primaryActionString
                          forState:UIControlStateNormal];
    UILabel* titleLabel = _primaryActionButton.titleLabel;
    titleLabel.adjustsFontSizeToFitWidth = YES;
    titleLabel.minimumScaleFactor = 0.7;
    _primaryActionButton.titleLabel.adjustsFontForContentSizeCategory = YES;
    _primaryActionButton.accessibilityIdentifier =
        kPromoStylePrimaryActionAccessibilityIdentifier;
    _primaryActionButton.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;
    [_primaryActionButton addTarget:self
                             action:@selector(didTapPrimaryActionButton)
                   forControlEvents:UIControlEventTouchUpInside];
  }
  return _primaryActionButton;
}

- (UITextView*)disclaimerView {
  if (!self.disclaimerText) {
    return nil;
  }
  if (!_disclaimerView) {
    // Set up disclaimer view.
    _disclaimerView = CreateUITextViewWithTextKit1();
    _disclaimerView.accessibilityIdentifier =
        kPromoStyleDisclaimerViewAccessibilityIdentifier;
    _disclaimerView.textContainerInset = UIEdgeInsetsMake(0, 0, 0, 0);
    _disclaimerView.scrollEnabled = NO;
    _disclaimerView.editable = NO;
    _disclaimerView.adjustsFontForContentSizeCategory = YES;
    _disclaimerView.delegate = self;
    _disclaimerView.backgroundColor = UIColor.clearColor;
    _disclaimerView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _disclaimerView.translatesAutoresizingMaskIntoConstraints = NO;
    _disclaimerView.attributedText = [self attributedStringForDisclaimer];
  }
  return _disclaimerView;
}

- (void)setDisclaimerText:(NSString*)disclaimerText {
  _disclaimerText = disclaimerText;
  NSAttributedString* attributedText = [self attributedStringForDisclaimer];
  if (attributedText) {
    self.disclaimerView.attributedText = attributedText;
  }
}

- (void)setDisclaimerURLs:(NSArray<NSURL*>*)disclaimerURLs {
  _disclaimerURLs = disclaimerURLs;
  NSAttributedString* attributedText = [self attributedStringForDisclaimer];
  if (attributedText) {
    self.disclaimerView.attributedText = attributedText;
  }
}

// Helper to create the learn more button.
- (UIButton*)learnMoreButton {
  if (!_learnMoreButton) {
    DCHECK(self.shouldShowLearnMoreButton);
    _learnMoreButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_learnMoreButton setImage:[UIImage imageNamed:@"help_icon"]
                      forState:UIControlStateNormal];
    _learnMoreButton.accessibilityIdentifier =
        kPromoStyleLearnMoreActionAccessibilityIdentifier;
    _learnMoreButton.translatesAutoresizingMaskIntoConstraints = NO;
    [_learnMoreButton addTarget:self
                         action:@selector(didTapLearnMoreButton)
               forControlEvents:UIControlEventTouchUpInside];
  }
  return _learnMoreButton;
}

#pragma mark - Private

// Updates banner constraints.
- (void)setupBannerConstraints {
  if (_scrollContentView == nil) {
    return;
  }

  if (_bannerConstraints != nil) {
    [NSLayoutConstraint deactivateConstraints:_bannerConstraints];
  }

  _bannerConstraints = @[
    // Common banner image constraints, further constraints are added below.
    // This one ensures the banner is well centered within the view.
    [self.bannerImageView.centerXAnchor
        constraintEqualToAnchor:self.view.centerXAnchor],
  ];

  if (self.shouldHideBanner) {
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [self.bannerImageView.heightAnchor constraintEqualToConstant:0],
      [self.bannerImageView.topAnchor
          constraintEqualToAnchor:_scrollContentView.topAnchor
                         constant:kDefaultMargin]
    ]];
  } else if (self.shouldBannerFillTopSpace) {
    NSLayoutDimension* dimFromToOfViewToBottomOfBanner = [self.view.topAnchor
        anchorWithOffsetToAnchor:self.bannerImageView.bottomAnchor];
    // Constrain bottom of banner to top of view + C * height of view
    // where C = isTallBanner ? tallMultiplier : defaultMultiplier.
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [dimFromToOfViewToBottomOfBanner
          constraintEqualToAnchor:self.view.heightAnchor
                       multiplier:[self bannerMultiplier]]
    ]];
  } else {
    // Default.
    _bannerConstraints = [_bannerConstraints arrayByAddingObjectsFromArray:@[
      [self.bannerImageView.topAnchor
          constraintEqualToAnchor:_scrollContentView.topAnchor],
    ]];
  }

  [NSLayoutConstraint activateConstraints:_bannerConstraints];
}

- (UIImage*)bannerImage {
  if (self.shouldHideBanner && !self.bannerName) {
    return [[UIImage alloc] init];
  }
  return [UIImage imageNamed:self.bannerName];
}

// Computes banner's image size.
- (CGSize)computeBannerImageSize {
  if (self.shouldHideBanner) {
    return CGSizeZero;
  }
  CGFloat bannerMultiplier = [self bannerMultiplier];
  CGFloat bannerAspectRatio =
      [self bannerImage].size.width / [self bannerImage].size.height;

  CGFloat destinationHeight = 0;
  CGFloat destinationWidth = 0;

  if (!self.shouldBannerFillTopSpace) {
    destinationHeight = roundf(self.view.bounds.size.height * bannerMultiplier);
    destinationWidth = roundf(bannerAspectRatio * destinationHeight);
  } else {
    CGFloat minBannerWidth = self.view.bounds.size.width;
    CGFloat minBannerHeight = self.view.bounds.size.height * bannerMultiplier;
    destinationWidth =
        roundf(fmax(minBannerWidth, bannerAspectRatio * minBannerHeight));
    destinationHeight = roundf(bannerAspectRatio * destinationWidth);
  }

  CGSize newSize = CGSizeMake(destinationWidth, destinationHeight);
  return newSize;
}

- (CGFloat)bannerMultiplier {
  switch (self.bannerSize) {
    case BannerImageSizeType::kStandard:
      return kDefaultBannerMultiplier;
    case BannerImageSizeType::kTall:
      return kTallBannerMultiplier;
    case BannerImageSizeType::kExtraTall:
      return kExtraTallBannerMultiplier;
  }
}

// Returns a new UIImage which is `sourceImage` resized to `newSize`. Returns
// `currentImage` if it is already at the correct size.
- (UIImage*)scaleBannerWithCurrentImage:(UIImage*)currentImage
                                 toSize:(CGSize)newSize {
  UIUserInterfaceStyle currentStyle =
      UITraitCollection.currentTraitCollection.userInterfaceStyle;
  if (CGSizeEqualToSize(newSize, currentImage.size) &&
      _bannerStyle == currentStyle) {
    return currentImage;
  }

  _bannerStyle = currentStyle;
  return ResizeImage([self bannerImage], newSize, ProjectionMode::kAspectFit);
}

// Determines which font text style to use depending on the device size, the
// size class and if dynamic type is enabled.
- (UIFontTextStyle)titleLabelFontTextStyle {
  UIViewController* presenter =
      self.presentingViewController ? self.presentingViewController : self;
  BOOL dynamicTypeEnabled = UIContentSizeCategoryIsAccessibilityCategory(
      presenter.traitCollection.preferredContentSizeCategory);

  if (!dynamicTypeEnabled) {
    if ([self isRegularXRegularSizeClass:presenter.traitCollection]) {
      return UIFontTextStyleTitle1;
    } else if (!IsSmallDevice()) {
      return UIFontTextStyleLargeTitle;
    }
  }
  return UIFontTextStyleTitle2;
}

- (void)setFontForTitle:(UILabel*)titleLabel {
  UIFontTextStyle textStyle = [self titleLabelFontTextStyle];

  UIFontDescriptor* descriptor =
      [UIFontDescriptor preferredFontDescriptorWithTextStyle:textStyle];
  UIFont* font = [UIFont systemFontOfSize:descriptor.pointSize
                                   weight:UIFontWeightBold];
  UIFontMetrics* fontMetrics = [UIFontMetrics metricsForTextStyle:textStyle];
  titleLabel.font = [fontMetrics scaledFontForFont:font];
}

- (void)setPrimaryActionButtonFont:(UIButton*)button {
  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
}

// Sets or resets the "Read More" text label when the bottom hasn't been
// reached yet and scrolling to the end is mandatory.
- (void)setReadMoreText {
  if (!self.scrollToEndMandatory) {
    return;
  }

  if (self.didReachBottom) {
    return;
  }

  if (!_canUpdateViewsOnScroll) {
    return;
  }

  DCHECK(self.readMoreString);
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kSolidButtonTextColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
  };

  NSMutableAttributedString* attributedString =
      [[NSMutableAttributedString alloc] initWithString:self.readMoreString
                                             attributes:textAttributes];

  // Use `ceilf()` when calculating the icon's bounds to ensure the
  // button's content height does not shrink by fractional points, as the
  // attributed string's actual height is slightly smaller than the
  // assigned height.
  NSTextAttachment* attachment = [[NSTextAttachment alloc] init];
  attachment.image = [[UIImage imageNamed:@"read_more_arrow"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  CGFloat height = ceilf(attributedString.size.height);
  CGFloat capHeight = ceilf(
      [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline].capHeight);
  CGFloat horizontalOffset =
      base::i18n::IsRTL() ? -1.f * kMoreArrowMargin : kMoreArrowMargin;
  CGFloat verticalOffset = (capHeight - height) / 2.f;
  attachment.bounds =
      CGRectMake(horizontalOffset, verticalOffset, height, height);
  [attributedString
      appendAttributedString:[NSAttributedString
                                 attributedStringWithAttachment:attachment]];
  self.primaryActionButton.accessibilityIdentifier =
      kPromoStyleReadMoreActionAccessibilityIdentifier;

  // Make the title change without animation, as the UIButton's default
  // animation when using setTitle:forState: doesn't handle adding a
  // UIImage well (the old title gets abruptly pushed to the side as it's
  // fading out to make room for the new image, which looks awkward).
  __weak PromoStyleViewController* weakSelf = self;
  [UIView performWithoutAnimation:^{
    [weakSelf.primaryActionButton setAttributedTitle:attributedString
                                            forState:UIControlStateNormal];
    [weakSelf.primaryActionButton layoutIfNeeded];
  }];
}

- (UIButton*)createButtonWithText:(NSString*)buttonText
          accessibilityIdentifier:(NSString*)accessibilityIdentifier {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:buttonText forState:UIControlStateNormal];
  [button setBackgroundColor:[UIColor clearColor]];
  UIColor* titleColor = [UIColor colorNamed:kBlueColor];
  [button setTitleColor:titleColor forState:UIControlStateNormal];

  // TODO(crbug.com/1418068): Replace with UIButtonConfiguration when min
  // deployment target is iOS 15.
  UIEdgeInsets contentInsets =
      UIEdgeInsetsMake(kButtonVerticalInsets, 0, kButtonVerticalInsets, 0);
  SetContentEdgeInsets(button, contentInsets);

  button.titleLabel.font =
      [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontForContentSizeCategory = YES;
  button.accessibilityIdentifier = accessibilityIdentifier;
  button.titleLabel.lineBreakMode = NSLineBreakByTruncatingTail;

  button.pointerInteractionEnabled = YES;
  button.pointerStyleProvider = CreateOpaqueButtonPointerStyleProvider();

  return button;
}

// Add the scroll view delegate and setup the content views. Should be done only
// once all the view layouts are fully done.
- (void)setupScrollView {
  _scrollView.delegate = self;
  _canUpdateViewsOnScroll = YES;

  // At this point, the scroll view has computed its content height. If
  // scrolling to the end is needed, and the entire content is already
  // fully visible (scrolled), set `didReachBottom` to YES. Otherwise, replace
  // the primary button's label with the read more label to indicate that more
  // scrolling is required.
  _scrollViewBottomOffsetY = _scrollView.contentSize.height -
                             _scrollView.bounds.size.height +
                             _scrollView.contentInset.bottom;

  BOOL isScrolledToBottom = [self isScrolledToBottom];
  _separator.hidden = isScrolledToBottom;
  if (self.didReachBottom || isScrolledToBottom) {
    [self updateActionButtonsAndPushUpScrollViewIfMandatory];
  } else {
    [self setReadMoreText];
  }
}

// Returns whether the scroll view's offset has reached the scroll view's
// content height, indicating that the scroll view has been fully scrolled.
- (BOOL)isScrolledToBottom {
  CGFloat scrollPosition =
      _scrollView.contentOffset.y + _scrollView.frame.size.height;
  CGFloat scrollLimit =
      _scrollView.contentSize.height + _scrollView.contentInset.bottom;
  return scrollPosition >= scrollLimit;
}

- (void)scrollToBottom {
  CGFloat scrollLimit = _scrollView.contentSize.height -
                        _scrollView.bounds.size.height +
                        _scrollView.contentInset.bottom;
  [_scrollView setContentOffset:CGPointMake(0, scrollLimit) animated:YES];
}

// If scrolling to the end of the content is mandatory, this method updates the
// action buttons based on whether the scroll view is currently scrolled to the
// end. If the scroll view has scrolled to the end, also sets `didReachBottom`.
// It also updates the separator visibility based on scroll position.
- (void)updateViewsOnScrollViewUpdate {
  if (!_canUpdateViewsOnScroll) {
    return;
  }

  BOOL isScrolledToBottom = [self isScrolledToBottom];
  _separator.hidden = isScrolledToBottom;
  if (self.scrollToEndMandatory && !self.didReachBottom && isScrolledToBottom) {
    [self updateActionButtonsAndPushUpScrollViewIfMandatory];
  }
}

// This method should be called right before the view is scrolled to the bottom.
// It updates the primary button's label and adds secondary and/or tertiary
// buttons, and as a result, pushing the scroll view up by updating the bottom
// offset of the scroll view and scroll to the new offset if the change in
// action buttons is triggered by a scroll in a view that sets
// `self.scrollToEndMandatory=YES`. It also sets `self.didReachBottom` to YES.
- (void)updateActionButtonsAndPushUpScrollViewIfMandatory {
  if (_buttonUpdated) {
    return;
  }
  _buttonUpdated = YES;
  HighlightButton* primaryActionButton = self.primaryActionButton;
  [primaryActionButton setAttributedTitle:nil forState:UIControlStateNormal];
  [primaryActionButton setTitle:self.primaryActionString
                       forState:UIControlStateNormal];
  primaryActionButton.accessibilityIdentifier =
      kPromoStylePrimaryActionAccessibilityIdentifier;
  // Reset the font to make sure it is properly scaled.
  [self setPrimaryActionButtonFont:primaryActionButton];

  // Add other buttons with the correct margins.
  if (self.secondaryActionString) {
    _secondaryActionButton = [self createSecondaryActionButton];
    [_actionStackView insertArrangedSubview:_secondaryActionButton atIndex:1];
  }
  if (self.tertiaryActionString) {
    _tertiaryActionButton = [self createTertiaryActionButton];
    [_actionStackView insertArrangedSubview:_tertiaryActionButton atIndex:0];
  }

  if (self.secondaryActionString || self.tertiaryActionString) {
    // Update constraints.
    [NSLayoutConstraint
        deactivateConstraints:_buttonsVerticalAnchorConstraints];
    _buttonsVerticalAnchorConstraints = @[
      [_scrollView.bottomAnchor
          constraintEqualToAnchor:_actionStackView.topAnchor
                         constant:self.tertiaryActionString ? 0
                                                            : -kDefaultMargin],
      [_actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.bottomAnchor
                                   constant:-kActionsBottomMargin],
      [_actionStackView.bottomAnchor
          constraintLessThanOrEqualToAnchor:self.view.safeAreaLayoutGuide
                                                .bottomAnchor],
    ];
    [NSLayoutConstraint activateConstraints:_buttonsVerticalAnchorConstraints];
  }
  if (self.scrollToEndMandatory) {
    _shouldScrollToBottom = YES;
  }
  self.didReachBottom = YES;
}

- (void)didTapPrimaryActionButton {
  if (self.scrollToEndMandatory && !self.didReachBottom) {
    // Calculate the offset needed to see the next content while keeping the
    // current content partially visible.
    CGFloat currentOffsetY = _scrollView.contentOffset.y;
    CGPoint targetOffset = CGPointMake(
        0, currentOffsetY + _scrollView.bounds.size.height *
                                (1.0 - kPreviousContentVisibleOnScroll));
    // Add one point to maximum possible offset to work around some issues when
    // the fonts are increased.
    if (targetOffset.y < _scrollViewBottomOffsetY + 1) {
      [_scrollView setContentOffset:targetOffset animated:YES];
    } else {
      [self updateActionButtonsAndPushUpScrollViewIfMandatory];
    }
  } else if ([self.delegate
                 respondsToSelector:@selector(didTapPrimaryActionButton)]) {
    [self.delegate didTapPrimaryActionButton];
  }
}

- (void)didTapSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapSecondaryActionButton)]) {
    [self.delegate didTapSecondaryActionButton];
  }
}

- (void)didTapTertiaryActionButton {
  DCHECK(self.tertiaryActionString);
  if ([self.delegate
          respondsToSelector:@selector(didTapTertiaryActionButton)]) {
    [self.delegate didTapTertiaryActionButton];
  }
}

// Handle taps on the help button.
- (void)didTapLearnMoreButton {
  DCHECK(self.shouldShowLearnMoreButton);
  if ([self.delegate respondsToSelector:@selector(didTapLearnMoreButton)]) {
    [self.delegate didTapLearnMoreButton];
  }
}

// Helper that returns whether the `traitCollection` has a regular vertical
// and regular horizontal size class.
// Copied from "ios/chrome/browser/shared/ui/util/uikit_ui_util.mm"
- (bool)isRegularXRegularSizeClass:(UITraitCollection*)traitCollection {
  return traitCollection.verticalSizeClass == UIUserInterfaceSizeClassRegular &&
         traitCollection.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

- (UIFontTextStyle)disclaimerLabelFontTextStyle {
  return UIFontTextStyleCaption2;
}

// Helper class that returns the an NSAttributedString generated from the
// current disclaimer text and URLs.
- (NSAttributedString*)attributedStringForDisclaimer {
  StringWithTags parsedString = ParseStringWithLinks(self.disclaimerText);
  if (parsedString.ranges.size() != [self.disclaimerURLs count]) {
    return nil;
  }

  NSMutableParagraphStyle* paragraphStyle =
      [[NSParagraphStyle defaultParagraphStyle] mutableCopy];
  paragraphStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:[self disclaimerLabelFontTextStyle]],
    NSParagraphStyleAttributeName : paragraphStyle
  };
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];
  size_t index = 0;
  for (NSURL* url in self.disclaimerURLs) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:url
                           range:parsedString.ranges[index]];
    index += 1;
  }
  return attributedText;
}

- (UIScrollView*)createScrollView {
  UIScrollView* scrollView = [[UIScrollView alloc] init];
  scrollView.translatesAutoresizingMaskIntoConstraints = NO;
  scrollView.accessibilityIdentifier =
      kPromoStyleScrollViewAccessibilityIdentifier;
  return scrollView;
}

- (UIView*)createFullHeaderImageView {
  switch (self.headerImageType) {
    case PromoStyleImageType::kAvatar: {
      UIImage* circleImage = [UIImage imageNamed:@"promo_style_avatar_circle"];
      DCHECK(circleImage);
      UIImageView* imageView = [[UIImageView alloc] initWithImage:circleImage];
      imageView.translatesAutoresizingMaskIntoConstraints = NO;
      return imageView;
    }
    case PromoStyleImageType::kImageWithShadow: {
      UIView* frameView = [[UIView alloc] init];
      frameView.translatesAutoresizingMaskIntoConstraints = NO;
      frameView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
      frameView.layer.cornerRadius = kHeaderImageCornerRadius;
      frameView.layer.shadowOffset =
          CGSizeMake(kHeaderImageShadowOffsetX, kHeaderImageShadowOffsetY);
      frameView.layer.shadowRadius = kHeaderImageShadowRadius;
      frameView.layer.shadowOpacity = kHeaderImageShadowOpacity;
      return frameView;
    }
    case PromoStyleImageType::kImage: {
      UIView* frameView = [[UIView alloc] init];
      frameView.translatesAutoresizingMaskIntoConstraints = NO;
      return frameView;
    }
    case PromoStyleImageType::kNone:
      NOTREACHED_NORETURN();
  }
}

- (UIImageView*)createheaderBackgroundImageView {
  CHECK(self.headerImageType != PromoStyleImageType::kNone);
  UIImageView* imageView =
      [[UIImageView alloc] initWithImage:self.headerBackgroundImage];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  imageView.accessibilityIdentifier =
      kPromoStyleHeaderViewBackgroundAccessibilityIdentifier;
  return imageView;
}

- (UILabel*)createSubtitleLabel {
  UILabel* subtitleLabel = [[UILabel alloc] init];
  subtitleLabel.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  subtitleLabel.numberOfLines = 0;
  subtitleLabel.textColor = [UIColor colorNamed:kGrey800Color];
  subtitleLabel.text = self.subtitleText;
  subtitleLabel.textAlignment = NSTextAlignmentCenter;
  subtitleLabel.translatesAutoresizingMaskIntoConstraints = NO;
  subtitleLabel.adjustsFontForContentSizeCategory = YES;
  subtitleLabel.accessibilityIdentifier =
      kPromoStyleSubtitleAccessibilityIdentifier;
  return subtitleLabel;
}

- (UIButton*)createSecondaryActionButton {
  DCHECK(self.secondaryActionString);
  UIButton* button = [self createButtonWithText:self.secondaryActionString
                        accessibilityIdentifier:
                            kPromoStyleSecondaryActionAccessibilityIdentifier];
  UILabel* titleLabel = button.titleLabel;
  titleLabel.adjustsFontSizeToFitWidth = YES;
  titleLabel.minimumScaleFactor = 0.7;

  [button addTarget:self
                action:@selector(didTapSecondaryActionButton)
      forControlEvents:UIControlEventTouchUpInside];

  return button;
}

- (UIButton*)createTertiaryActionButton {
  DCHECK(self.tertiaryActionString);
  UIButton* button = [self
         createButtonWithText:self.tertiaryActionString
      accessibilityIdentifier:kPromoStyleTertiaryActionAccessibilityIdentifier];
  [button addTarget:self
                action:@selector(didTapTertiaryActionButton)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateViewsOnScrollViewUpdate];
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView == self.disclaimerView &&
      [self.delegate respondsToSelector:@selector(didTapURLInDisclaimer:)]) {
    [self.delegate didTapURLInDisclaimer:URL];
  }
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable. Another solution is
  // to subclass `UITextView` and override `canBecomeFirstResponder` to return
  // NO, but that workaround only works on iOS 13.5+. This is the simplest
  // approach that works well on iOS 12, 13 & 14.
  textView.selectedTextRange = nil;
}

@end
