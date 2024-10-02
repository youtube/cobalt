// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

#import <ostream>

#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/notreached.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/bubble/bubble_util.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Accessibility identifier for the close button.
NSString* const kBubbleViewCloseButtonIdentifier =
    @"BubbleViewCloseButtonIdentifier";
// Accessibility identifier for the title label.
NSString* const kBubbleViewTitleLabelIdentifier =
    @"BubbleViewTitleLabelIdentifier";
// Accessibility identifier for the label.
NSString* const kBubbleViewLabelIdentifier = @"BubbleViewLabelIdentifier";
// Accessibility identifier for the image view.
NSString* const kBubbleViewImageViewIdentifier =
    @"BubbleViewImageViewIdentifier";
// Accessibility identifier for the snooze button.
NSString* const kBubbleViewSnoozeButtonIdentifier =
    @"kBubbleViewSnoozeButtonIdentifier";
// Accessibility identifier for the arrow view.
NSString* const kBubbleViewArrowViewIdentifier =
    @"kBubbleViewArrowViewIdentifier";

namespace {
// The color of the bubble (both circular background and arrow).
UIColor* BubbleColor() {
  return [UIColor colorNamed:kBlueColor];
}

// The corner radius of the bubble's background, which causes the ends of the
// badge to be circular.
const CGFloat kBubbleCornerRadius = 15.0f;
// Margin between the bubble view's bounds and its content. Vertical for top and
// bottom margins, Horizontal for leading and trailing margins.
const CGFloat kBubbleVerticalMargin = 4.0f;
const CGFloat kBubbleHorizontalMargin = 16.0f;
// Padding between the top and bottom the bubble's background and the top and
// bottom of its content.
const CGFloat kBubbleVerticalPadding = 16.0f;
// Padding between the sides of the bubble's background and the sides of its
// content.
const CGFloat kBubbleHorizontalPadding = 16.0f;

// The size that the arrow will appear to have.
const CGSize kArrowSize = {32, 9};
// Margin to ensure that the arrow is not outside of the background.
const CGFloat kArrowMargin = kArrowSize.width / 2.0f;

// The offset of the bubble's drop shadow, which will be slightly below the
// bubble.
const CGSize kShadowOffset = {0.0f, 2.0f};
// The blur radius of the bubble's drop shadow.
const CGFloat kShadowRadius = 4.0f;
// The opacity of the bubble's drop shadow.
const CGFloat kShadowOpacity = 0.1f;

// Bezier curve constants.
const CGFloat kControlPointCenter = 0.243125;
const CGFloat kControlPointEnd = 0.514375;

// The size of the close button.
const CGFloat kCloseButtonSize = 48.0f;
// The padding for the top and trailing edges of the close button.
const CGFloat kCloseButtonTopTrailingPadding = 15.0f;

// Margin between title and label.
const CGFloat kTitleBottomMargin = 3.0f;

// Margin between the imageView its leading and trailing sides.
const CGFloat kImageViewLeadingMargin = 16.0f;
const CGFloat kImageViewTrailingMargin = 12.0f;
// Height and Width of imageView.
const CGFloat kImageViewSize = 60.0f;
// Corner radius of imageView.
const CGFloat kImageViewCornerRadius = 13.0f;

// The top and bottom margin of the title in snooze button.
const CGFloat kSnoozeButtonTitleVerticalMargin = 16.0f;
const CGFloat kSnoozeButtonMinimumSize = 48.0f;
const CGFloat kSnoozeButtonFontSize = 15.0f;

// The size of symbol action images.
const CGFloat kSymbolBubblePointSize = 17;

// Returns a background view for BubbleView.
UIView* BubbleBackgroundView() {
  UIView* background = [[UIView alloc] initWithFrame:CGRectZero];
  [background setBackgroundColor:BubbleColor()];
  [background.layer setCornerRadius:kBubbleCornerRadius];
  [background setTranslatesAutoresizingMaskIntoConstraints:NO];
  return background;
}

// Returns an arrow view for BubbleView.
UIView* BubbleArrowViewWithDirection(BubbleArrowDirection arrowDirection) {
  CGFloat width = kArrowSize.width;
  CGFloat height = kArrowSize.height;
  UIView* arrow =
      [[UIView alloc] initWithFrame:CGRectMake(0.0f, 0.0f, width, height)];
  UIBezierPath* path = UIBezierPath.bezierPath;
  CGFloat xCenter = width / 2;
  CGFloat controlPointCenter = xCenter * kControlPointCenter;
  CGFloat controlPointEnd = xCenter * kControlPointEnd;
  if (arrowDirection == BubbleArrowDirectionUp) {
    [path moveToPoint:CGPointMake(xCenter, 0)];
    [path addCurveToPoint:CGPointMake(width, height)
            controlPoint1:CGPointMake(xCenter + controlPointCenter, 0)
            controlPoint2:CGPointMake(xCenter + controlPointEnd, height)];
    [path addLineToPoint:CGPointMake(0, height)];
    [path addCurveToPoint:CGPointMake(xCenter, 0)
            controlPoint1:CGPointMake(xCenter - controlPointEnd, height)
            controlPoint2:CGPointMake(xCenter - controlPointCenter, 0)];
  } else {
    [path moveToPoint:CGPointMake(xCenter, height)];
    [path addCurveToPoint:CGPointMake(width, 0)
            controlPoint1:CGPointMake(xCenter + controlPointCenter, height)
            controlPoint2:CGPointMake(xCenter + controlPointEnd, 0)];
    [path addLineToPoint:CGPointZero];
    [path addCurveToPoint:CGPointMake(xCenter, height)
            controlPoint1:CGPointMake(xCenter - controlPointEnd, 0)
            controlPoint2:CGPointMake(xCenter - controlPointCenter, height)];
  }
  [path closePath];
  CAShapeLayer* layer = [CAShapeLayer layer];
  [layer setPath:path.CGPath];
  [layer setFillColor:BubbleColor().CGColor];
  [arrow.layer addSublayer:layer];
  [arrow setAccessibilityIdentifier:kBubbleViewArrowViewIdentifier];
  [arrow setTranslatesAutoresizingMaskIntoConstraints:NO];
  return arrow;
}

// Returns a close button for BubbleView.
UIButton* BubbleCloseButton() {
  UIImage* buttonImage =
      DefaultSymbolWithPointSize(kXMarkSymbol, kSymbolBubblePointSize);
  // Computes the paddings to position the button's image. The button is
  // bigger than the image for accessibility purposes.
  const CGFloat closeButtonBottomPadding = kCloseButtonSize -
                                           kCloseButtonTopTrailingPadding -
                                           buttonImage.size.height;
  const CGFloat closeButtonLeadingPadding = kCloseButtonSize -
                                            kCloseButtonTopTrailingPadding -
                                            buttonImage.size.width;
  UIButton* button;

  // TODO(crbug.com/1418068): Simplify after minimum version required is >=
  // iOS 15.
  if (base::ios::IsRunningOnIOS15OrLater() &&
      IsUIButtonConfigurationEnabled()) {
    if (@available(iOS 15, *)) {
      UIButtonConfiguration* buttonConfiguration =
          [UIButtonConfiguration plainButtonConfiguration];
      [buttonConfiguration setImage:buttonImage];
      [buttonConfiguration
          setContentInsets:NSDirectionalEdgeInsetsMake(
                               kCloseButtonTopTrailingPadding,
                               closeButtonLeadingPadding,
                               closeButtonBottomPadding,
                               kCloseButtonTopTrailingPadding)];
      button = [UIButton buttonWithConfiguration:buttonConfiguration
                                   primaryAction:nil];
    }
  } else {
    button = [UIButton buttonWithType:UIButtonTypeSystem];
    [button setImage:buttonImage forState:UIControlStateNormal];
    [button.imageView setBounds:CGRectZero];
    [button.imageView setContentMode:UIViewContentModeScaleAspectFit];
    UIEdgeInsets contentEdgeInsets = UIEdgeInsetsMakeDirected(
        kCloseButtonTopTrailingPadding, closeButtonLeadingPadding,
        closeButtonBottomPadding, kCloseButtonTopTrailingPadding);
    SetImageEdgeInsets(button, contentEdgeInsets);
  }
  [button setTintColor:[UIColor colorNamed:kSolidButtonTextColor]];
  [button setAccessibilityLabel:l10n_util::GetNSString(IDS_IOS_ICON_CLOSE)];
  [button setAccessibilityIdentifier:kBubbleViewCloseButtonIdentifier];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  return button;
}

// Returns a snooze button for BubbleView.
UIButton* BubbleSnoozeButton(
    UIControlContentHorizontalAlignment buttonAlignment) {
  UIButton* button = [UIButton buttonWithType:UIButtonTypeSystem];
  [button setTitle:l10n_util::GetNSString(IDS_IOS_IPH_BUBBLE_SNOOZE)
          forState:UIControlStateNormal];
  [button setTitleColor:[UIColor colorNamed:kSolidButtonTextColor]
               forState:UIControlStateNormal];
  [button.titleLabel
      setFont:[UIFont boldSystemFontOfSize:kSnoozeButtonFontSize]];
  [button.titleLabel setNumberOfLines:0];
  [button.titleLabel setLineBreakMode:NSLineBreakByWordWrapping];
  [button setContentHorizontalAlignment:buttonAlignment];
  [button setAccessibilityIdentifier:kBubbleViewSnoozeButtonIdentifier];
  [button setTranslatesAutoresizingMaskIntoConstraints:NO];
  return button;
}

// Returns a label to be used for a BubbleView that displays white text.
UILabel* BubbleLabelWithText(NSString* text, NSTextAlignment textAlignment) {
  DCHECK(text.length);
  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  [label setText:text];
  [label setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline]];
  [label setTextColor:[UIColor colorNamed:kSolidButtonTextColor]];
  [label setTextAlignment:textAlignment];
  [label setNumberOfLines:0];
  [label setLineBreakMode:NSLineBreakByWordWrapping];
  [label setTranslatesAutoresizingMaskIntoConstraints:NO];
  return label;
}

// Returns a label to be used for the BubbleView's title.
UILabel* BubbleTitleLabelWithText(NSString* text,
                                  NSTextAlignment textAlignment) {
  DCHECK(text.length);
  UILabel* label = BubbleLabelWithText(text, textAlignment);
  [label setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]];
  [label setAccessibilityIdentifier:kBubbleViewTitleLabelIdentifier];
  return label;
}

// Returns a image view used for the BubbleViews's imageView.
UIImageView* BubbleImageViewWithImage(UIImage* image) {
  UIImageView* imageView = [[UIImageView alloc] initWithImage:image];
  [imageView.layer setCornerRadius:kImageViewCornerRadius];
  [imageView.layer setMasksToBounds:YES];
  [imageView setContentMode:UIViewContentModeCenter];
  [imageView setAccessibilityIdentifier:kBubbleViewImageViewIdentifier];
  imageView.translatesAutoresizingMaskIntoConstraints = NO;
  return imageView;
}

}  // namespace

@interface BubbleView ()
// Label containing the text displayed on the bubble.
@property(nonatomic, strong, readonly) UILabel* label;
// Label containing the title displayed on the bubble.
@property(nonatomic, strong, readonly) UILabel* titleLabel;
// Pill-shaped view in the background of the bubble.
@property(nonatomic, strong, readonly) UIView* background;
// Triangular arrow that points to the target UI element.
@property(nonatomic, strong, readonly) UIView* arrow;
// Optional close button displayed at the trailing top corner of the bubble.
@property(nonatomic, strong, readonly) UIButton* closeButton;
// Optional snooze button displayed on the bubble.
@property(nonatomic, strong, readonly) UIButton* snoozeButton;
// Optional image displayed at the leading edge of the bubble.
@property(nonatomic, strong, readonly) UIImageView* imageView;
// Triangular shape, the backing layer for the arrow.
@property(nonatomic, weak) CAShapeLayer* arrowLayer;
@property(nonatomic, assign, readonly) BubbleArrowDirection direction;
@property(nonatomic, assign, readonly) BubbleAlignment alignment;
// Constraint for the arrow horizontal alignment.
@property(nonatomic, strong) NSLayoutConstraint* arrowAlignmentConstraint;
// Indicate whether views' constraints need to be added to the bubble.
@property(nonatomic, assign) BOOL needsAddConstraints;

// Controls if there is a close button in the view.
@property(nonatomic, readonly) BOOL showsCloseButton;
// Controls if there is a snooze button in the view.
@property(nonatomic, readonly) BOOL showsSnoozeButton;
// The delegate for interactions in this View.
@property(nonatomic, weak, readonly) id<BubbleViewDelegate> delegate;

@end

@implementation BubbleView

- (instancetype)initWithText:(NSString*)text
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment
            showsCloseButton:(BOOL)shouldShowCloseButton
                       title:(NSString*)titleString
                       image:(UIImage*)image
           showsSnoozeButton:(BOOL)shouldShowSnoozeButton
               textAlignment:(NSTextAlignment)textAlignment
                    delegate:(id<BubbleViewDelegate>)delegate {
  self = [super initWithFrame:CGRectZero];
  if (self) {
    _direction = direction;
    _alignment = alignment;
    _alignmentOffset = bubble_util::BubbleDefaultAlignmentOffset();
    // Add background view.
    _background = BubbleBackgroundView();
    [self addSubview:_background];
    // Add label view.
    _label = BubbleLabelWithText(text, textAlignment);
    _label.accessibilityIdentifier = kBubbleViewLabelIdentifier;
    [self addSubview:_label];
    // Add arrow view.
    _arrow = BubbleArrowViewWithDirection(direction);
    _arrowLayer = [_arrow.layer.sublayers lastObject];
    [self addSubview:_arrow];
    // Add title label if present.
    if (titleString && titleString.length > 0) {
      _titleLabel = BubbleTitleLabelWithText(titleString, textAlignment);
      [_label
          setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]];
      [self addSubview:_titleLabel];
    }
    // Add image view if present.
    if (image) {
      _imageView = BubbleImageViewWithImage(image);
      [self addSubview:_imageView];
    }
    // Add close button if present.
    _showsCloseButton = shouldShowCloseButton;
    if (_showsCloseButton) {
      _closeButton = BubbleCloseButton();
      [_closeButton addTarget:self
                       action:@selector(closeButtonWasTapped:)
             forControlEvents:UIControlEventTouchUpInside];
      [self addSubview:_closeButton];
    }
    // Add snooze button if present.
    _showsSnoozeButton = shouldShowSnoozeButton;
    if (_showsSnoozeButton) {
      UIControlContentHorizontalAlignment buttonAlignment =
          textAlignment == NSTextAlignmentCenter
              ? UIControlContentHorizontalAlignmentCenter
              : UIControlContentHorizontalAlignmentLeading;
      _snoozeButton = BubbleSnoozeButton(buttonAlignment);
      [_snoozeButton addTarget:self
                        action:@selector(snoozeButtonWasTapped:)
              forControlEvents:UIControlEventTouchUpInside];
      [self addSubview:_snoozeButton];
    }
    _delegate = delegate;
    _needsAddConstraints = YES;

    self.isAccessibilityElement = YES;
  }
  return self;
}

- (instancetype)initWithText:(NSString*)text
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment {
  return [self initWithText:text
             arrowDirection:direction
                  alignment:alignment
           showsCloseButton:NO
                      title:nil
                      image:nil
          showsSnoozeButton:NO
              textAlignment:NSTextAlignmentCenter
                   delegate:nil];
}

#pragma mark - Public

- (void)setAlignmentOffset:(CGFloat)alignmentOffset {
  _alignmentOffset = alignmentOffset;
  [self updateArrowAlignmentConstraint];
}

- (NSString*)accessibilityLabel {
  return self.titleLabel.text;
}

- (NSString*)accessibilityValue {
  return self.label.text;
}

- (NSArray<UIAccessibilityCustomAction*>*)accessibilityCustomActions {
  NSMutableArray<UIAccessibilityCustomAction*>* accessibilityCustomActions =
      [NSMutableArray array];
  if (self.showsSnoozeButton) {
    [accessibilityCustomActions
        addObject:[[UIAccessibilityCustomAction alloc]
                      initWithName:self.snoozeButton.accessibilityLabel
                            target:self
                          selector:@selector(snoozeButtonWasTapped:)]];
  }
  if (self.showsCloseButton) {
    [accessibilityCustomActions
        addObject:[[UIAccessibilityCustomAction alloc]
                      initWithName:self.closeButton.accessibilityLabel
                            target:self
                          selector:@selector(closeButtonWasTapped:)]];
  }
  return accessibilityCustomActions;
}

#pragma mark - Private instance methods

// Handles taps on the close button.
- (void)closeButtonWasTapped:(UIButton*)button {
  DCHECK(self.showsCloseButton);
  if ([self.delegate respondsToSelector:@selector(didTapCloseButton)]) {
    [self.delegate didTapCloseButton];
  }
}

// Handles taps on the snooze button.
- (void)snoozeButtonWasTapped:(UIButton*)button {
  DCHECK(self.showsSnoozeButton);
  if ([self.delegate respondsToSelector:@selector(didTapSnoozeButton)]) {
    [self.delegate didTapSnoozeButton];
  }
}

// Add a drop shadow to the bubble.
- (void)addShadow {
  [self.layer setShadowOffset:kShadowOffset];
  [self.layer setShadowRadius:kShadowRadius];
  [self.layer setShadowColor:[UIColor blackColor].CGColor];
  [self.layer setShadowOpacity:kShadowOpacity];
}

#pragma mark - View's constraints

// Activate Autolayout constraints to properly position the bubble's subviews.
- (void)activateConstraints {
  // Add constraints that do not depend on the bubble's direction or alignment.
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:[self generalConstraints]];
  // Add the constraint that aligns the arrow relative to the bubble if none was
  // added before.
  if (!self.arrowAlignmentConstraint) {
    [self updateArrowAlignmentConstraint];
  }
  // Add constraints that depend on the bubble's direction.
  [constraints addObjectsFromArray:[self arrowDirectionConstraints]];
  // Add constraints for close button.
  if (self.showsCloseButton) {
    [constraints addObjectsFromArray:[self closeButtonConstraints]];
  }
  // Add constraints for title label.
  if (self.titleLabel) {
    [constraints addObjectsFromArray:[self titleLabelConstraints]];
  }
  // Add constraints for image view.
  if (self.imageView) {
    [constraints addObjectsFromArray:[self imageViewConstraints]];
  }
  // Add constraints for snooze button.
  if (self.showsSnoozeButton) {
    [constraints addObjectsFromArray:[self snoozeButtonConstraints]];
  }
  [NSLayoutConstraint activateConstraints:constraints];
}

// Return an array of constraints that do not depend on the bubble's arrow
// direction or alignment.
- (NSArray<NSLayoutConstraint*>*)generalConstraints {
  UIView* background = self.background;
  UIView* label = self.label;
  UIView* arrow = self.arrow;
  // Ensure that the label is top aligned and properly aligned horizontally.
  NSArray<NSLayoutConstraint*>* labelAlignmentConstraints = @[
    [label.topAnchor constraintEqualToAnchor:background.topAnchor
                                    constant:kBubbleVerticalPadding],
    [label.leadingAnchor constraintEqualToAnchor:background.leadingAnchor
                                        constant:kBubbleHorizontalPadding],
    [background.trailingAnchor
        constraintEqualToAnchor:label.trailingAnchor
                       constant:kBubbleHorizontalPadding],
  ];
  for (NSLayoutConstraint* constraint in labelAlignmentConstraints) {
    constraint.priority = UILayoutPriorityDefaultLow;
  }
  // Add horizontal margins between the bubble's frame and the background. These
  // constraints are optional (if the bubble is too close to the edge of the
  // screen, the margin is ignored), they shouldn't affect the arrow's position.
  NSArray<NSLayoutConstraint*>* bubbleMarginConstraints = @[
    [background.leadingAnchor constraintEqualToAnchor:self.leadingAnchor
                                             constant:kBubbleHorizontalMargin],
    [self.trailingAnchor constraintEqualToAnchor:background.trailingAnchor
                                        constant:kBubbleHorizontalMargin],
  ];
  for (NSLayoutConstraint* constraint in bubbleMarginConstraints) {
    constraint.priority = UILayoutPriorityDefaultHigh;
  }
  // Make sure that if one of the bubbleMarginConstraints is overridden, the
  // bubble still stays centered.
  NSLayoutConstraint* backgroundCentering =
      [self.centerXAnchor constraintEqualToAnchor:background.centerXAnchor];
  bubbleMarginConstraints =
      [bubbleMarginConstraints arrayByAddingObject:backgroundCentering];
  // Ensure that the arrow is inside the background's bound. These constraints
  // shouldn't affect the arrow's position.
  NSArray<NSLayoutConstraint*>* bubbleArrowMarginConstraints = @[
    [arrow.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:background.leadingAnchor
                                    constant:kArrowMargin],
    [background.trailingAnchor
        constraintGreaterThanOrEqualToAnchor:arrow.trailingAnchor
                                    constant:kArrowMargin],
  ];
  for (NSLayoutConstraint* constraint in bubbleArrowMarginConstraints) {
    constraint.priority = UILayoutPriorityDefaultHigh + 1;
  }
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        // Ensure the background view is smaller than `self.view`.
        [background.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:self.leadingAnchor],
        [self.trailingAnchor
            constraintGreaterThanOrEqualToAnchor:background.trailingAnchor],
        // Ensure that the background view is as wide as the label, with added
        // padding on the sides of the label.
        [label.topAnchor
            constraintGreaterThanOrEqualToAnchor:background.topAnchor
                                        constant:kBubbleVerticalPadding],
        [background.bottomAnchor
            constraintGreaterThanOrEqualToAnchor:label.bottomAnchor
                                        constant:kBubbleVerticalPadding],
        [label.leadingAnchor
            constraintGreaterThanOrEqualToAnchor:background.leadingAnchor
                                        constant:kBubbleHorizontalPadding],
        [background.trailingAnchor
            constraintGreaterThanOrEqualToAnchor:label.trailingAnchor
                                        constant:kBubbleHorizontalPadding],
        // Enforce the arrow's size, scaling by `kArrowScaleFactor` to prevent
        // gaps between the arrow and the background view.
        [arrow.widthAnchor constraintEqualToConstant:kArrowSize.width],
        [arrow.heightAnchor constraintEqualToConstant:kArrowSize.height]
      ]];
  [constraints addObjectsFromArray:labelAlignmentConstraints];
  [constraints addObjectsFromArray:bubbleMarginConstraints];
  [constraints addObjectsFromArray:bubbleArrowMarginConstraints];
  return constraints;
}

// Returns the constraint for the close button.
- (NSArray<NSLayoutConstraint*>*)closeButtonConstraints {
  UIView* closeButton = self.closeButton;
  NSArray<NSLayoutConstraint*>* constraints = @[
    [closeButton.widthAnchor constraintEqualToConstant:kCloseButtonSize],
    [closeButton.heightAnchor constraintEqualToConstant:kCloseButtonSize],
    [closeButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:self.label.trailingAnchor],
    [closeButton.topAnchor constraintEqualToAnchor:self.background.topAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:self.background.trailingAnchor],
  ];
  return constraints;
}

// Returns the constraint for the title label.
- (NSArray<NSLayoutConstraint*>*)titleLabelConstraints {
  UIView* titleLabel = self.titleLabel;
  UIView* label = self.label;
  NSArray<NSLayoutConstraint*>* constraints = @[
    [titleLabel.topAnchor constraintEqualToAnchor:self.background.topAnchor
                                         constant:kBubbleVerticalPadding],
    [titleLabel.leadingAnchor constraintEqualToAnchor:label.leadingAnchor],
    [titleLabel.trailingAnchor constraintEqualToAnchor:label.trailingAnchor],
    [label.topAnchor constraintEqualToAnchor:titleLabel.bottomAnchor
                                    constant:kTitleBottomMargin],
  ];
  return constraints;
}

// Returns the constraint for the image view.
- (NSArray<NSLayoutConstraint*>*)imageViewConstraints {
  UIView* imageView = self.imageView;
  UIView* background = self.background;
  NSArray<NSLayoutConstraint*>* constraints = @[
    [imageView.widthAnchor constraintEqualToConstant:kImageViewSize],
    [imageView.heightAnchor constraintEqualToConstant:kImageViewSize],
    [imageView.topAnchor
        constraintGreaterThanOrEqualToAnchor:background.topAnchor
                                    constant:kBubbleVerticalPadding],
    [background.bottomAnchor
        constraintGreaterThanOrEqualToAnchor:imageView.bottomAnchor
                                    constant:kBubbleVerticalPadding],
    [imageView.centerYAnchor constraintEqualToAnchor:background.centerYAnchor],
    [imageView.leadingAnchor constraintEqualToAnchor:background.leadingAnchor
                                            constant:kImageViewLeadingMargin],
    [self.label.leadingAnchor constraintEqualToAnchor:imageView.trailingAnchor
                                             constant:kImageViewTrailingMargin],
  ];
  return constraints;
}

// Returns the constraint for the snooze button.
- (NSArray<NSLayoutConstraint*>*)snoozeButtonConstraints {
  UIView* background = self.background;
  UIView* label = self.label;
  UIButton* snoozeButton = self.snoozeButton;
  NSMutableArray<NSLayoutConstraint*>* constraints =
      [NSMutableArray arrayWithArray:@[
        [snoozeButton.titleLabel.topAnchor
            constraintEqualToAnchor:label.bottomAnchor
                           constant:kSnoozeButtonTitleVerticalMargin],
        [snoozeButton.leadingAnchor
            constraintEqualToAnchor:label.leadingAnchor],
        [background.bottomAnchor
            constraintEqualToAnchor:snoozeButton.titleLabel.bottomAnchor
                           constant:kSnoozeButtonTitleVerticalMargin],
        [background.trailingAnchor
            constraintGreaterThanOrEqualToAnchor:snoozeButton.trailingAnchor
                                        constant:kBubbleHorizontalPadding],
        [snoozeButton.heightAnchor
            constraintGreaterThanOrEqualToConstant:kSnoozeButtonMinimumSize],
        [snoozeButton.widthAnchor
            constraintGreaterThanOrEqualToConstant:kSnoozeButtonMinimumSize],
      ]];
  if (self.showsCloseButton) {
    [constraints
        addObject:[snoozeButton.trailingAnchor
                      constraintLessThanOrEqualToAnchor:self.closeButton
                                                            .leadingAnchor]];
  }
  return constraints;
}

// Returns the constraint that aligns the arrow to the bubble view. This depends
// on the bubble's alignment.
- (NSLayoutConstraint*)arrowAlignmentConstraintWithOffset:
    (CGFloat)alignmentOffset {
  // The anchor of the bubble which is aligned with the arrow's center anchor.
  NSLayoutAnchor* anchor;
  // The constant by which `anchor` is offset from the arrow's center anchor.
  CGFloat offset;
  switch (self.alignment) {
    case BubbleAlignmentLeading:
      // The anchor point is at a distance of `alignmentOffset`
      // from the bubble's leading edge. Center align the arrow with the anchor
      // point by aligning the center of the arrow with the leading edge of the
      // bubble view and adding an offset of `alignmentOffset`.
      anchor = self.leadingAnchor;
      offset = alignmentOffset;
      break;
    case BubbleAlignmentCenter:
      // Since the anchor point is at the center of the bubble view, center the
      // arrow on the bubble view.
      anchor = self.centerXAnchor;
      offset = 0.0f;
      break;
    case BubbleAlignmentTrailing:
      // The anchor point is at a distance of `alignmentOffset`
      // from the bubble's trailing edge. Center align the arrow with the anchor
      // point by aligning the center of the arrow with the trailing edge of the
      // bubble view and adding an offset of `-alignmentOffset`.
      anchor = self.trailingAnchor;
      offset = -alignmentOffset;
      break;
    default:
      NOTREACHED() << "Invalid bubble alignment " << self.alignment;
      return nil;
  }
  return
      [self.arrow.centerXAnchor constraintEqualToAnchor:anchor constant:offset];
}

// Return an array of constraints that depend on the bubble's arrow direction.
- (NSArray<NSLayoutConstraint*>*)arrowDirectionConstraints {
  NSArray<NSLayoutConstraint*>* constraints;
  if (self.direction == BubbleArrowDirectionUp) {
    constraints = @[
      [self.background.topAnchor constraintEqualToAnchor:self.arrow.topAnchor
                                                constant:kArrowSize.height],
      // Ensure that the top of the arrow is aligned with the top of the bubble
      // view and add a margin above the arrow.
      [self.arrow.topAnchor constraintEqualToAnchor:self.topAnchor
                                           constant:kBubbleVerticalMargin]
    ];
  } else {
    DCHECK(self.direction == BubbleArrowDirectionDown);
    constraints = @[
      [self.arrow.bottomAnchor
          constraintEqualToAnchor:self.background.bottomAnchor
                         constant:kArrowSize.height],
      // Ensure that the bottom of the arrow is aligned with the bottom of the
      // bubble view and add a margin below the arrow.
      [self.bottomAnchor constraintEqualToAnchor:self.arrow.bottomAnchor
                                        constant:kBubbleVerticalMargin]
    ];
  }
  return constraints;
}

- (void)updateArrowAlignmentConstraint {
  if (self.arrowAlignmentConstraint) {
    self.arrowAlignmentConstraint.active = NO;
  }
  self.arrowAlignmentConstraint =
      [self arrowAlignmentConstraintWithOffset:self.alignmentOffset];
  self.arrowAlignmentConstraint.active = YES;
}

#pragma mark - UIView overrides

// Override `willMoveToSuperview` to add view properties to the view hierarchy.
- (void)willMoveToSuperview:(UIView*)newSuperview {
  // If constraints have not been added to the view, add them.
  if (self.needsAddConstraints) {
    [self activateConstraints];
    // Add drop shadow.
    [self addShadow];
    // Set `needsAddConstraints` to NO to ensure that the constraints are only
    // added to the view hierarchy once.
    self.needsAddConstraints = NO;
  }
  [super willMoveToSuperview:newSuperview];
}

// Calculates the optimal size of the text (label, title and snooze button's
// label) with the available size to minimize whitespace. Returns the size of
// the combined UI element including padding between texts.
- (CGSize)optimalTextSize:(CGSize)size {
  // Computes sizeThatFits for label, title and snoozeButton's label.
  CGSize labelSize = [self.label sizeThatFits:size];
  CGSize titleSize = CGSizeZero;
  if (self.titleLabel) {
    titleSize = [self.titleLabel sizeThatFits:size];
    titleSize.height += kTitleBottomMargin;
  }
  CGSize snoozeButtonTitleSize = CGSizeZero;
  if (self.showsSnoozeButton) {
    snoozeButtonTitleSize = [self.snoozeButton.titleLabel sizeThatFits:size];
    // Add padding to computed height.
    snoozeButtonTitleSize.height += kSnoozeButtonTitleVerticalMargin;
  }
  // Optimal width is the maximum width between label, title and snoozeButton's
  // label.
  CGFloat textWidth = MAX(labelSize.width, titleSize.width);
  textWidth = MAX(textWidth, snoozeButtonTitleSize.width);
  CGFloat textHeight =
      labelSize.height + titleSize.height + snoozeButtonTitleSize.height;
  CGSize textSize = CGSizeMake(textWidth, textHeight);
  return textSize;
}

// Override `sizeThatFits` to return the bubble's optimal size. Calculate
// optimal size by finding the labels' optimal size, and adding inset distances
// to the labels' dimensions. This method also enforces minimum bubble width to
// prevent strange, undesired behaviors, and maximum labels width to preserve
// readability.
- (CGSize)sizeThatFits:(CGSize)size {
  // The combined horizontal inset distance of the label and title with respect
  // to the bubble.
  CGFloat textHorizontalInset = kBubbleHorizontalMargin * 2;
  // Add close button size, which is on the trailing edge of the labels.
  if (self.showsCloseButton) {
    textHorizontalInset += MAX(kCloseButtonSize, kBubbleHorizontalPadding);
  } else {
    textHorizontalInset += kBubbleHorizontalPadding;
  }
  // Add image view size, which is on the leading edge of the labels.
  if (self.imageView) {
    textHorizontalInset +=
        kImageViewLeadingMargin + kImageViewSize + kImageViewTrailingMargin;
  } else {
    textHorizontalInset += kBubbleHorizontalPadding;
  }
  CGFloat textMaxWidth = size.width - textHorizontalInset;
  CGSize optimalTextSize =
      [self optimalTextSize:CGSizeMake(textMaxWidth, size.height)];

  // Ensure that the bubble is at least as wide as the minimum bubble width.
  CGFloat bubbleWidth =
      MAX(optimalTextSize.width + textHorizontalInset, [self minBubbleWidth]);
  // Calculate the height needed to display the bubble.
  // Combined height of title, label and snooze button including all margins.
  CGFloat textContentHeight = kBubbleVerticalPadding + optimalTextSize.height;
  if (self.showsSnoozeButton) {
    textContentHeight +=
        MAX(kBubbleVerticalPadding, kSnoozeButtonTitleVerticalMargin);
  } else {
    textContentHeight += kBubbleVerticalPadding;
  }
  // Height of image including all margins.
  CGFloat imageContentHeight =
      self.imageView ? 2 * kBubbleVerticalPadding + kImageViewSize : 0.0f;
  // Calculates the height needed to display the bubble.
  CGFloat bubbleHeight = 2 * kBubbleVerticalMargin + kArrowSize.height +
                         MAX(imageContentHeight, textContentHeight);
  CGSize bubbleSize = CGSizeMake(bubbleWidth, bubbleHeight);
  return bubbleSize;
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if ([self.traitCollection
          hasDifferentColorAppearanceComparedToTraitCollection:
              previousTraitCollection]) {
    self.arrowLayer.fillColor = BubbleColor().CGColor;
  }
}

#pragma mark - Private sizes

// The minimum bubble width is two times the bubble alignment offset, which
// causes the bubble to appear center-aligned for short display text.
- (CGFloat)minBubbleWidth {
  return self.alignmentOffset * 2;
}

@end
