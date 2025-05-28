// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_profile_edit_table_view_controller_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_profile_edit_table_view_controller_delegate.h"

namespace autofill {
class AutofillProfile;
class PersonalDataManager;
}  // namespace autofill

@protocol AutofillProfileEditConsumer;
@protocol AutofillProfileEditMediatorDelegate;
@class CountryItem;

// The Mediator for viewing and editing the profile.
@interface AutofillProfileEditMediator
    : NSObject <AutofillProfileEditTableViewControllerDelegate,
                AutofillSettingsProfileEditTableViewControllerDelegate>

// Designated initializer. `AutofillProfileEditMediatorDelegate` and
// `dataManager` should not be nil.
- (instancetype)initWithDelegate:
                    (id<AutofillProfileEditMediatorDelegate>)delegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager
                 autofillProfile:(autofill::AutofillProfile*)profile
               isMigrationPrompt:(BOOL)isMigrationPrompt
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Consumer of this mediator.
@property(nonatomic, weak) id<AutofillProfileEditConsumer> consumer;

// Called when the country is selected from a dropdown.
- (void)didSelectCountry:(CountryItem*)countryItem;

// Returns YES if the view can be dismissed immediately.
- (BOOL)canDismissImmediately;

// Returns YES if a confirmation dialog should be shown when the view is
// dismissed.
- (BOOL)shouldShowConfirmationDialogOnDismissBySwiping;

// Saves the profile and dismisses the view.
- (void)saveChangesForDismiss;

@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_UI_BUNDLED_ADDRESS_EDITOR_AUTOFILL_PROFILE_EDIT_MEDIATOR_H_
