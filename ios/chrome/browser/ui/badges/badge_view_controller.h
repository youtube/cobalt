// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_consumer.h"
#import "ios/chrome/browser/ui/fullscreen/fullscreen_ui_element.h"

@class BadgeButtonFactory;
@class LayoutGuideCenter;

// Manages badges to display that are received through BadgeConsumer. Currently
// only displays the newest badge.
@interface BadgeViewController
    : UIViewController <BadgeConsumer, FullscreenUIElement>

// The layout guide center to use to reference the displayed badge.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

// `buttonFactory` must be non-nil.
- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_VIEW_CONTROLLER_H_
