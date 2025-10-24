// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_LEGACY_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_LEGACY_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/manage_accounts_consumer.h"
#import "ios/chrome/browser/settings/ui_bundled/google_services/manage_accounts/with_overridable_model_identity_data_source.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_navigation_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

@protocol ApplicationCommands;
@protocol ManageAccountsModelIdentityDataSource;
@protocol SettingsCommands;
class Browser;

// TableView that handles the settings for accounts when the user is signed in
// to Chrome.
@interface LegacyAccountsTableViewController
    : SettingsRootTableViewController <ManageAccountsConsumer,
                                       SettingsControllerProtocol,
                                       WithOverridableModelIdentityDataSource>

// Model delegate.
@property(nonatomic, weak) id<ManageAccountsModelIdentityDataSource>
    modelIdentityDataSource;

// `browser` must not be nil.
// If `closeSettingsOnAddAccount` is YES, then this account table view
// controller will close the settings view when an account is added.
- (instancetype)initWithBrowser:(Browser*)browser
              closeSettingsOnAddAccount:(BOOL)closeSettingsOnAddAccount
             applicationCommandsHandler:
                 (id<ApplicationCommands>)applicationCommandsHandler
    signoutDismissalByParentCoordinator:
        (BOOL)signoutDismissalByParentCoordinator NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_LEGACY_ACCOUNTS_TABLE_VIEW_CONTROLLER_H_
