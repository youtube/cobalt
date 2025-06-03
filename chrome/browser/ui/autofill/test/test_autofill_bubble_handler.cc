// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test/test_autofill_bubble_handler.h"

#include "chrome/browser/ui/autofill/payments/save_iban_ui.h"

namespace autofill {

TestAutofillBubbleHandler::TestAutofillBubbleHandler() = default;

TestAutofillBubbleHandler::~TestAutofillBubbleHandler() = default;

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveCreditCardBubble(
    content::WebContents* web_contents,
    SaveCardBubbleController* controller,
    bool is_user_gesture) {
  if (!save_card_bubble_view_)
    save_card_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_card_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowIbanBubble(
    content::WebContents* web_contents,
    IbanBubbleController* controller,
    bool is_user_gesture,
    IbanBubbleType bubble_type) {
  if (!iban_bubble_view_) {
    iban_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return iban_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowLocalCardMigrationBubble(
    content::WebContents* web_contents,
    LocalCardMigrationBubbleController* controller,
    bool is_user_gesture) {
  if (!local_card_migration_bubble_view_) {
    local_card_migration_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return local_card_migration_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowOfferNotificationBubble(
    content::WebContents* web_contents,
    OfferNotificationBubbleController* controller,
    bool is_user_gesture) {
  if (!offer_notification_bubble_view_)
    offer_notification_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return offer_notification_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowSaveAddressProfileBubble(
    content::WebContents* contents,
    SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  if (!save_address_profile_bubble_view_)
    save_address_profile_bubble_view_ = std::make_unique<TestAutofillBubble>();
  return save_address_profile_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowUpdateAddressProfileBubble(
    content::WebContents* contents,
    SaveUpdateAddressProfileBubbleController* controller,
    bool is_user_gesture) {
  if (!update_address_profile_bubble_view_) {
    update_address_profile_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return update_address_profile_bubble_view_.get();
}

AutofillBubbleBase*
TestAutofillBubbleHandler::ShowVirtualCardManualFallbackBubble(
    content::WebContents* web_contents,
    VirtualCardManualFallbackBubbleController* controller,
    bool is_user_gesture) {
  if (!virtual_card_manual_fallback_bubble_view_) {
    virtual_card_manual_fallback_bubble_view_ =
        std::make_unique<TestAutofillBubble>();
  }
  return virtual_card_manual_fallback_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowVirtualCardEnrollBubble(
    content::WebContents* web_contents,
    VirtualCardEnrollBubbleController* controller,
    bool is_user_gesture) {
  if (!virtual_card_enroll_bubble_view_) {
    virtual_card_enroll_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return virtual_card_enroll_bubble_view_.get();
}

AutofillBubbleBase* TestAutofillBubbleHandler::ShowMandatoryReauthBubble(
    content::WebContents* web_contents,
    MandatoryReauthBubbleController* controller,
    bool is_user_gesture,
    MandatoryReauthBubbleType bubble_type) {
  if (!mandatory_reauth_bubble_view_) {
    mandatory_reauth_bubble_view_ = std::make_unique<TestAutofillBubble>();
  }
  return mandatory_reauth_bubble_view_.get();
}

}  // namespace autofill
