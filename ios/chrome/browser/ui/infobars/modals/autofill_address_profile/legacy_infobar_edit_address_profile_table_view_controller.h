// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/shared/ui/table_view/chrome_table_view_controller.h"
#import "ios/chrome/browser/ui/infobars/modals/autofill_address_profile/infobar_edit_address_profile_modal_consumer.h"

@protocol LegacyInfobarEditAddressProfileModalDelegate;

// The TableView for an Autofill save/update address edit menu.
@interface LegacyInfobarEditAddressProfileTableViewController
    : ChromeTableViewController <InfobarEditAddressProfileModalConsumer>

- (instancetype)initWithModalDelegate:
    (id<LegacyInfobarEditAddressProfileModalDelegate>)delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_MODALS_AUTOFILL_ADDRESS_PROFILE_LEGACY_INFOBAR_EDIT_ADDRESS_PROFILE_TABLE_VIEW_CONTROLLER_H_
