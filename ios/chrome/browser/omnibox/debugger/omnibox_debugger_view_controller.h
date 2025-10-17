// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/omnibox/debugger/omnibox_debugger_consumer.h"

/// View controller used to display omnibox and popup related debug info.
@interface PopupDebugInfoViewController
    : UIViewController <OmniboxDebuggerConsumer>

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_OMNIBOX_DEBUGGER_OMNIBOX_DEBUGGER_VIEW_CONTROLLER_H_
