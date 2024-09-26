// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"

#import "components/autofill/core/browser/autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/overlays/public/infobar_banner/save_address_profile_infobar_banner_overlay_request_config.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill_address_profile_infobar_overlays::
    SaveAddressProfileBannerRequestConfig;

#pragma mark - InfobarBannerInteractionHandler

SaveAddressProfileInfobarBannerInteractionHandler::
    SaveAddressProfileInfobarBannerInteractionHandler()
    : InfobarBannerInteractionHandler(
          SaveAddressProfileBannerRequestConfig::RequestSupport()) {}

SaveAddressProfileInfobarBannerInteractionHandler::
    ~SaveAddressProfileInfobarBannerInteractionHandler() = default;

void SaveAddressProfileInfobarBannerInteractionHandler::BannerVisibilityChanged(
    InfoBarIOS* infobar,
    bool visible) {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS* delegate =
      autofill::AutofillSaveUpdateAddressProfileDelegateIOS::
          FromInfobarDelegate(infobar->delegate());
  delegate->set_is_infobar_visible(visible);
  if (!visible) {
    delegate->MessageTimeout();
  }
}

void SaveAddressProfileInfobarBannerInteractionHandler::BannerDismissedByUser(
    InfoBarIOS* infobar) {
  autofill::AutofillSaveUpdateAddressProfileDelegateIOS::FromInfobarDelegate(
      infobar->delegate())
      ->MessageDeclined();
}
