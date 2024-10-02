// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/overlays/public/infobar_modal/save_address_profile_infobar_modal_overlay_request_config.h"

#import "base/check.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/common/infobars/infobar_overlay_request_config.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill_address_profile_infobar_overlays {

OVERLAY_USER_DATA_SETUP_IMPL(SaveAddressProfileModalRequestConfig);

SaveAddressProfileModalRequestConfig::SaveAddressProfileModalRequestConfig(
    InfoBarIOS* infobar)
    : infobar_(infobar) {
  DCHECK(infobar_);
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());

  address_ = delegate->GetEnvelopeStyleAddress();
  emailAddress_ = delegate->GetEmailAddress();
  phoneNumber_ = delegate->GetPhoneNumber();
  current_address_profile_saved_ = infobar->accepted();
  profile_diff_ = [[NSMutableDictionary alloc] init];

  if (IsUpdateModal()) {
    StoreProfileDiff(delegate->GetProfileDiff());
    update_modal_description_ = delegate->GetSubtitle();
  }

  current_address_profile_saved_ = infobar->accepted();
  is_migration_to_account_ = delegate->IsMigrationToAccount();
  syncing_user_email_ = delegate->SyncingUserEmail();
  is_profile_an_account_profile_ = delegate->IsProfileAnAccountProfile();
  profile_description_for_migration_prompt_ =
      delegate->GetProfileDescriptionForMigrationPrompt();
}

SaveAddressProfileModalRequestConfig::~SaveAddressProfileModalRequestConfig() =
    default;

bool SaveAddressProfileModalRequestConfig::IsUpdateModal() const {
  return static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
             infobar_->delegate())
      ->GetOriginalProfile();
}

void SaveAddressProfileModalRequestConfig::StoreProfileDiff(
    const std::vector<autofill::ProfileValueDifference>& profile_diff) {
  for (const auto& row : profile_diff) {
    [profile_diff_
        setObject:@[
          base::SysUTF16ToNSString(row.first_value),
          base::SysUTF16ToNSString(row.second_value)
        ]
           forKey:[NSNumber
                      numberWithInt:AutofillUITypeFromAutofillType(row.type)]];
  }
}

NSDictionary* SaveAddressProfileModalRequestConfig::GetProfileInfo() {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());
  NSMutableDictionary* items = [[NSMutableDictionary alloc] init];
  for (const auto& type : GetAutofillTypeForProfileEdit()) {
    [items setObject:base::SysUTF16ToNSString(delegate->GetProfileInfo(type))
              forKey:[NSNumber
                         numberWithInt:AutofillUITypeFromAutofillType(type)]];
  }
  return items;
}

const autofill::AutofillProfile*
SaveAddressProfileModalRequestConfig::GetProfile() {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      static_cast<autofill::AutofillSaveUpdateAddressProfileDelegateIOS*>(
          infobar_->delegate());
  return delegate->GetProfile();
}

void SaveAddressProfileModalRequestConfig::CreateAuxiliaryData(
    base::SupportsUserData* user_data) {
  InfobarOverlayRequestConfig::CreateForUserData(
      user_data, infobar_, InfobarOverlayType::kModal, false);
}

}  // namespace autofill_address_profile_infobar_overlays
