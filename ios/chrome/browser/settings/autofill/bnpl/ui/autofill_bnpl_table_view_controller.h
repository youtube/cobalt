// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/autofill/bnpl/ui/autofill_bnpl_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@class AutofillBnplTableViewController;

// Delegate for AutofillBnplTableViewController.
@protocol AutofillBnplTableViewControllerDelegate
// Informs the delegate that the BNPL switch has changed value.
- (void)viewController:(AutofillBnplTableViewController*)controller
    didChangeBnplSwitchTo:(BOOL)isOn;
@end

// This class is responsible for displaying the Buy Now Pay Later
// settings page.
@interface AutofillBnplTableViewController
    : SettingsRootTableViewController <AutofillBnplConsumer>

// The delegate for this view controller.
@property(nonatomic, weak) id<AutofillBnplTableViewControllerDelegate> delegate;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_AUTOFILL_BNPL_UI_AUTOFILL_BNPL_TABLE_VIEW_CONTROLLER_H_
