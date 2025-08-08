// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_

#import <UIKit/UIKit.h>

// InfobarBannerPositioner contains methods used to position the InfobarBanner.
@protocol InfobarBannerPositioner

// The vertical positioning of the infobar banner in window coordinates.
- (CGFloat)bannerYPosition;

// The InfobarBanner view that will be presented. Used to calculate the
// intrinsic size of the content in order to set the container view height
// appropriately.
- (UIView*)bannerView;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_
