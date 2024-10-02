// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_

#import <Foundation/Foundation.h>

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol BrowsingDataCommands;

// Protocol allowing the dispatcher to be passed to the settings ViewController.
@protocol SettingsRootViewControlling

// The dispatcher used by this ViewController.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>
        dispatcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_VIEW_CONTROLLING_H_
