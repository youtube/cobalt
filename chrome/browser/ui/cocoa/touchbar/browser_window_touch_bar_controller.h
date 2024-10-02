// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_TOUCH_BAR_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_TOUCH_BAR_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#include <os/availability.h>

class Browser;
@class BrowserWindowDefaultTouchBar;
@class WebTextfieldTouchBarController;

namespace content {
class WebContents;
}

// Provides a touch bar for the browser window. This class implements the
// NSTouchBarDelegate and handles the items in the touch bar.
@interface BrowserWindowTouchBarController : NSObject

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window;

// Creates and returns a touch bar for the browser window.
- (NSTouchBar*)makeTouchBar;

// Invalidate the browser window touch bar by setting |window_|'s touch bar to
// nil.
- (void)invalidateTouchBar;

@end

@interface BrowserWindowTouchBarController (ExposedForTesting)

- (BrowserWindowDefaultTouchBar*)defaultTouchBar;

- (WebTextfieldTouchBarController*)webTextfieldTouchBar;
@end

#endif  // CHROME_BROWSER_UI_COCOA_TOUCHBAR_BROWSER_WINDOW_TOUCH_BAR_CONTROLLER_H_
