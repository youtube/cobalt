// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/web/print/web_state_printer.h"

// Interface for printing.
@interface PrintController : NSObject <WebStatePrinter>

// `baseViewController` is the default VC to present print preview in case it
// is not specified in the command.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Shows print UI for `view` with `title`.
// Print preview will be presented on top of `baseViewController`.
- (void)printView:(UIView*)view
             withTitle:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Shows print UI for `image` with `title`.
// Print preview will be presented on top of `baseViewController`.
- (void)printImage:(UIImage*)image
                 title:(NSString*)title
    baseViewController:(UIViewController*)baseViewController;

// Dismisses the print dialog with animation if `animated`.
- (void)dismissAnimated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_PRINT_PRINT_CONTROLLER_H_
