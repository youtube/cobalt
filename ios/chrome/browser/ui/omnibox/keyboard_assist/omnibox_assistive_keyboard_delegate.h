// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
@protocol BrowserCoordinatorCommands;
@class LayoutGuideCenter;
@protocol LensCommands;
@class OmniboxTextFieldIOS;
@protocol QRScannerCommands;

// Delegate protocol for the KeyboardAccessoryView.
@protocol OmniboxAssistiveKeyboardDelegate

// The layout guide center for the current scene.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// Notifies the delegate that the Voice Search button was tapped.
- (void)keyboardAccessoryVoiceSearchTapped:(id)sender;

// Notifies the delegate that the Camera Search button was tapped.
- (void)keyboardAccessoryCameraSearchTapped;

// Notifies the delegate that the Lens button was tapped.
- (void)keyboardAccessoryLensTapped;

// Notifies the delegate that a key with the title `title` was pressed.
- (void)keyPressed:(NSString*)title;

@end

// TODO(crbug.com/784819): Move this code to omnibox.
// Implementation of the OmniboxAssistiveKeyboardDelegate.
@interface OmniboxAssistiveKeyboardDelegateImpl
    : NSObject <OmniboxAssistiveKeyboardDelegate>

@property(nonatomic, weak) id<ApplicationCommands> applicationCommandsHandler;
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorCommandsHandler;
@property(nonatomic, weak) id<LensCommands> lensCommandsHandler;
@property(nonatomic, weak) id<QRScannerCommands> qrScannerCommandsHandler;
@property(nonatomic, weak) OmniboxTextFieldIOS* omniboxTextField;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_KEYBOARD_ASSIST_OMNIBOX_ASSISTIVE_KEYBOARD_DELEGATE_H_
