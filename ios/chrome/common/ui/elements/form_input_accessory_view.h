// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
#define IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_

#import <UIKit/UIKit.h>

@class FormInputAccessoryView;
@class FormInputAccessoryViewTextData;

// Informs the receiver of actions in the accessory view.
@protocol FormInputAccessoryViewDelegate
- (void)formInputAccessoryViewDidTapNextButton:(FormInputAccessoryView*)sender;
- (void)formInputAccessoryViewDidTapPreviousButton:
    (FormInputAccessoryView*)sender;
- (void)formInputAccessoryViewDidTapCloseButton:(FormInputAccessoryView*)sender;
- (FormInputAccessoryViewTextData*)textDataforFormInputAccessoryView:
    (FormInputAccessoryView*)sender;
- (void)fromInputAccessoryViewDidTapOmniboxTypingShield:
    (FormInputAccessoryView*)sender;
@end

extern NSString* const kFormInputAccessoryViewAccessibilityID;
extern NSString* const
    kFormInputAccessoryViewOmniboxTypingShieldAccessibilityID;

// Subview of the accessory view for web forms. Shows a custom view with form
// navigation controls above the keyboard. Enables input clicks by way of the
// playInputClick method.
@interface FormInputAccessoryView : UIView <UIInputViewAudioFeedback>

// The previous button if the view was set up with a navigation delegate. Nil
// otherwise.
@property(nonatomic, readonly, weak) UIButton* previousButton;

// The next button if the view was set up with a navigation delegate. Nil
// otherwise.
@property(nonatomic, readonly, weak) UIButton* nextButton;

// The leading view.
@property(nonatomic, readonly, weak) UIView* leadingView;

// Sets up the view with the given `leadingView`. Navigation controls are shown
// on the trailing side and use `delegate` for actions.
- (void)setUpWithLeadingView:(UIView*)leadingView
          navigationDelegate:(id<FormInputAccessoryViewDelegate>)delegate;

// Sets up the view with the given `leadingView`. Navigation controls are
// replaced with `customTrailingView`.
- (void)setUpWithLeadingView:(UIView*)leadingView
          customTrailingView:(UIView*)customTrailingView;

// Sets the height of the omnibox typing shield. Set a height of 0 to hide the
// typing shield. The omnibox typing shield is a transparent view on the top
// edge of the input accessory view for the collapsed bottom omnibox
// (crbug.com/1490601).
- (void)setOmniboxTypingShieldHeight:(CGFloat)typingShieldHeight;

@end

#endif  // IOS_CHROME_COMMON_UI_ELEMENTS_FORM_INPUT_ACCESSORY_VIEW_H_
