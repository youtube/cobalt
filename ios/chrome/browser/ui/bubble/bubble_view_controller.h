// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/bubble/bubble_view.h"

// View controller that manages a BubbleView, which points to a UI element of
// interest.
@interface BubbleViewController : UIViewController

// Initializes the bubble with the given text, titleString, image, arrow
// direction, alignment, type of bubble view and bubble view's delegate (handles
// bubble view's buttons taps).
- (instancetype)initWithText:(NSString*)text
                       title:(NSString*)titleString
                       image:(UIImage*)image
              arrowDirection:(BubbleArrowDirection)direction
                   alignment:(BubbleAlignment)alignment
              bubbleViewType:(BubbleViewType)type
                    delegate:(id<BubbleViewDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Animates the bubble view in with a fade-in and sink-down animation.
//
// The caller is responsible for adding the bubble view controller to the
// view hierarchy.
- (void)animateContentIn;

// Dismisses the bubble. If `animated` is true, the bubble fades out.
//
// The bubble view controller is automatically removed from the view hierarchy.
- (void)dismissAnimated:(BOOL)animated;

// Changes the bubbleView's alignment offset, this might change the bubbleView's
// size.
- (void)setBubbleAlignmentOffset:(CGFloat)alignmentOffset;

@end

#endif  // IOS_CHROME_BROWSER_UI_BUBBLE_BUBBLE_VIEW_CONTROLLER_H_
