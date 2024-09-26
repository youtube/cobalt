// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_delegate_impl.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_autofill_driver_injector.h"
#include "components/autofill/content/browser/test_autofill_manager_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/content/browser/test_content_autofill_driver.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/mock_fast_checkout_client.h"
#include "components/unified_consent/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class FastCheckoutDelegateImplTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Creates the AutofillDriver and AutofillManager.
    NavigateAndCommit(GURL("about:blank"));

    fast_checkout_delegate_ = std::make_unique<FastCheckoutDelegateImpl>(
        web_contents(), &fast_checkout_client_, autofill_manager());
  }

  autofill::TestContentAutofillClient* autofill_client() {
    return test_autofill_client_injector_[web_contents()];
  }

  autofill::TestContentAutofillDriver* autofill_driver() {
    return test_autofill_driver_injector_[web_contents()];
  }

  autofill::TestBrowserAutofillManager* autofill_manager() {
    return test_autofill_manager_injector_[web_contents()];
  }

  autofill::test::AutofillUnitTestEnvironment autofill_test_environment_;
  testing::NiceMock<MockFastCheckoutClient> fast_checkout_client_;
  std::unique_ptr<FastCheckoutDelegateImpl> fast_checkout_delegate_;
  autofill::TestAutofillClientInjector<autofill::TestContentAutofillClient>
      test_autofill_client_injector_;
  autofill::TestAutofillDriverInjector<autofill::TestContentAutofillDriver>
      test_autofill_driver_injector_;
  autofill::TestAutofillManagerInjector<autofill::TestBrowserAutofillManager>
      test_autofill_manager_injector_;
};

TEST_F(FastCheckoutDelegateImplTest, TryToShowFastCheckoutSucceeds) {
  EXPECT_CALL(fast_checkout_client_, TryToStart)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(fast_checkout_delegate_->TryToShowFastCheckout(
      autofill::FormData(), autofill::FormFieldData(), nullptr));
}

TEST_F(FastCheckoutDelegateImplTest, IsShowingFastCheckoutUI) {
  EXPECT_CALL(fast_checkout_client_, IsShowing).WillOnce(testing::Return(true));
  EXPECT_TRUE(fast_checkout_delegate_->IsShowingFastCheckoutUI());
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutWhenShowing) {
  EXPECT_CALL(fast_checkout_client_, IsShowing).WillOnce(testing::Return(true));
  EXPECT_CALL(fast_checkout_client_, Stop(false));
  fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
}

TEST_F(FastCheckoutDelegateImplTest, HideFastCheckoutWhenNotShowing) {
  EXPECT_CALL(fast_checkout_client_, IsShowing)
      .WillOnce(testing::Return(false));
  EXPECT_CALL(fast_checkout_client_, Stop(false)).Times(0);
  fast_checkout_delegate_->HideFastCheckout(/*allow_further_runs=*/false);
}

TEST_F(FastCheckoutDelegateImplTest, IntendsToShowFastCheckout) {
  autofill::FormData form;
  autofill::test::CreateTestAddressFormData(&form);
  autofill::FormFieldData field = form.fields[0];
  autofill::FormFieldData non_seen_field;
  autofill::test::CreateTestFormField("First Name", "firstname", "", "text",
                                      &non_seen_field);
  autofill_manager()->OnFormsSeen(
      /*updated_forms=*/{form},
      /*removed_forms=*/{});

  EXPECT_CALL(fast_checkout_client_, IsSupported)
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(fast_checkout_delegate_->IntendsToShowFastCheckout(
      *autofill_manager(), form.global_id(), field.global_id()));
  EXPECT_FALSE(fast_checkout_delegate_->IntendsToShowFastCheckout(
      *autofill_manager(), form.global_id(), non_seen_field.global_id()));
}
