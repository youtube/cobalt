// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/autofill_address_profile/save_address_profile_infobar_banner_interaction_handler.h"

#import "base/guid.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_autofill_save_update_address_profile_delegate_ios.h"
#import "ios/chrome/browser/infobars/overlays/infobar_overlay_request_inserter.h"
#import "ios/chrome/browser/infobars/test/fake_infobar_ios.h"
#import "ios/chrome/browser/infobars/test/mock_infobar_delegate.h"
#import "ios/chrome/browser/overlays/public/overlay_request_queue.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for SaveAddressProfileInfobarBannerInteractionHandler.
class SaveAddressProfileInfobarBannerInteractionHandlerTest
    : public PlatformTest {
 public:
  SaveAddressProfileInfobarBannerInteractionHandlerTest()
      : delegate_factory_(),
        profile_(base::GenerateGUID(), "https://www.example.com/") {
    infobar_ = std::make_unique<InfoBarIOS>(
        InfobarType::kInfobarTypeSaveAutofillAddressProfile,
        MockAutofillSaveUpdateAddressProfileDelegateIOSFactory::
            CreateMockAutofillSaveUpdateAddressProfileDelegateIOSFactory(
                profile_));
  }

  MockAutofillSaveUpdateAddressProfileDelegateIOS& mock_delegate() {
    return *static_cast<MockAutofillSaveUpdateAddressProfileDelegateIOS*>(
        infobar_->delegate());
  }

 protected:
  SaveAddressProfileInfobarBannerInteractionHandler handler_;
  MockAutofillSaveUpdateAddressProfileDelegateIOSFactory delegate_factory_;
  autofill::AutofillProfile profile_;
  std::unique_ptr<InfoBarIOS> infobar_;
};

// Tests that user_decision is set to message timeout on BannerVisibilityChanged
// with parameter value as false.
TEST_F(SaveAddressProfileInfobarBannerInteractionHandlerTest,
       BannerVisibilityFalse) {
  handler_.BannerVisibilityChanged(infobar_.get(), false);
  EXPECT_EQ(mock_delegate().user_decision(),
            autofill::AutofillClient::SaveAddressProfileOfferUserDecision::
                kMessageTimeout);
}

// Tests that user_decision is set to message declined on BannerDismissedByUser.
TEST_F(SaveAddressProfileInfobarBannerInteractionHandlerTest,
       BannerDismissedByUser) {
  handler_.BannerDismissedByUser(infobar_.get());
  EXPECT_EQ(mock_delegate().user_decision(),
            autofill::AutofillClient::SaveAddressProfileOfferUserDecision::
                kMessageDeclined);

  handler_.BannerVisibilityChanged(infobar_.get(), false);
  // Expect the user decision to be message declined even when
  // BannerVisibilityChanged is called.
  EXPECT_EQ(mock_delegate().user_decision(),
            autofill::AutofillClient::SaveAddressProfileOfferUserDecision::
                kMessageDeclined);
}
