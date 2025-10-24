// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_WITH_OVERRIDABLE_MODEL_IDENTITY_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_WITH_OVERRIDABLE_MODEL_IDENTITY_DATA_SOURCE_H_

@protocol ManageAccountsModelIdentityDataSource;

// Protocol exposing a settable property -modelIdentityDataSource.
@protocol WithOverridableModelIdentityDataSource

@property(nonatomic, weak) id<ManageAccountsModelIdentityDataSource>
    modelIdentityDataSource;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_GOOGLE_SERVICES_MANAGE_ACCOUNTS_WITH_OVERRIDABLE_MODEL_IDENTITY_DATA_SOURCE_H_
