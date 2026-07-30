// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_ZERO_STATE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_ZERO_STATE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// View controller for the Assistant AIM zero state.
@interface AssistantAIMZeroStateViewController : UIViewController

// The greeting message to display.
@property(nonatomic, copy) NSString* greetingMessage;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_UI_ASSISTANT_AIM_ZERO_STATE_VIEW_CONTROLLER_H_
