// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_URL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_URL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class GURL;
@class AIMSRPDebuggerURLViewController;

// Delegate protocol for AIMSRPDebuggerURLViewController.
@protocol AIMSRPDebuggerURLViewControllerDelegate <NSObject>

// Called when the user updates the URL in the debugger.
- (void)debuggerURLViewController:
            (AIMSRPDebuggerURLViewController*)viewController
                     didUpdateURL:(const GURL&)url;

@end

// A view controller that displays the loaded AIM URL with copy capabilities.
@interface AIMSRPDebuggerURLViewController : UIViewController

// The delegate to receive updates from the debugger.
@property(nonatomic, weak) id<AIMSRPDebuggerURLViewControllerDelegate> delegate;

- (instancetype)initWithURL:(const GURL&)url NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COBROWSE_DEBUGGER_AIM_SRP_DEBUGGER_URL_VIEW_CONTROLLER_H_
