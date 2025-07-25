// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>
#import "ios/chrome/browser/ui/whats_new/data_source/whats_new_item.h"

@protocol WhatsNewDetailViewDelegate;
@protocol WhatsNewDetailViewActionHandler;

class GURL;

// A base view controller for showing a What's New feature or chrome tip in
// detail.
@interface WhatsNewDetailViewController : UIViewController

// The init method used with What's New banner image, title, subtitle, primary
// action title, instructions setps, and has primary action.
- (instancetype)initWithParams:(UIImage*)image
                         title:(NSString*)title
                      subtitle:(NSString*)subtitle
            primaryActionTitle:(NSString*)primaryActionTitle
              instructionSteps:(NSArray<NSString*>*)instructionSteps
                          type:(WhatsNewType)type
                 primaryAction:(WhatsNewPrimaryAction)primaryAction
                  learnMoreURL:(const GURL&)learnMoreURL
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The delegate object that manages interactions with the primary action.
@property(nonatomic, weak) id<WhatsNewDetailViewActionHandler> actionHandler;

// The delegate object to this coordinator's parent.
@property(nonatomic, weak) id<WhatsNewDetailViewDelegate> delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_WHATS_NEW_WHATS_NEW_DETAIL_VIEW_CONTROLLER_H_
