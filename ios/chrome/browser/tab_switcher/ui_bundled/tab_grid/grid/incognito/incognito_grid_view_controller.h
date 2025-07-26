// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/incognito_reauth/ui_bundled/incognito_reauth_consumer.h"
#import "ios/chrome/browser/tab_switcher/ui_bundled/tab_grid/grid/base_grid_view_controller.h"

@protocol IncognitoReauthCommands;
@protocol GridCommands;

// Grid view controller for incognito grid. This class will handle every grid's
// features that are only available in incognito grid.
@interface IncognitoGridViewController
    : BaseGridViewController <IncognitoReauthConsumer>

// Handler for reauth commands.
@property(nonatomic, weak) id<IncognitoReauthCommands> reauthHandler;
// Handler for grid content commands.
@property(nonatomic, weak) id<GridCommands> gridHandler;
// YES when the current contents are hidden from the user before a successful
// biometric authentication.
@property(nonatomic, assign) BOOL contentNeedsAuthentication;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_VIEW_CONTROLLER_H_
