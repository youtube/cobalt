// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/account_picker/account_picker_selection/account_picker_selection_screen_consumer.h"

@protocol AccountPickerSelectionScreenTableViewControllerActionDelegate;
@protocol AccountPickerSelectionScreenTableViewControllerModelDelegate;

// View controller for AccountPickerSelectionScreenCoordinator.
@interface AccountPickerSelectionScreenTableViewController
    : ChromeTableViewController <AccountPickerSelectionScreenConsumer>

@property(nonatomic, weak)
    id<AccountPickerSelectionScreenTableViewControllerActionDelegate>
        actionDelegate;
@property(nonatomic, weak)
    id<AccountPickerSelectionScreenTableViewControllerModelDelegate>
        modelDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_ACCOUNT_PICKER_ACCOUNT_PICKER_SELECTION_ACCOUNT_PICKER_SELECTION_SCREEN_TABLE_VIEW_CONTROLLER_H_
