// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/authentication/ui_bundled/views/views_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"

// TableViewIdentityItem holds the model data for an identity.
@interface TableViewIdentityItem : TableViewItem

// Gaia ID.
@property(nonatomic, strong) NSString* gaiaID;
// User name.
@property(nonatomic, strong) NSString* name;
// User email.
@property(nonatomic, strong) NSString* email;
// User avatar.
@property(nonatomic, strong) UIImage* avatar;
// If YES, the identity is selected.
@property(nonatomic, assign) BOOL selected;
// If YES, the identity is managed.
@property(nonatomic, assign) BOOL managed;
// Style for the IdentityView.
@property(nonatomic, assign) IdentityViewStyle identityViewStyle;

@end

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CELLS_TABLE_VIEW_IDENTITY_ITEM_H_
