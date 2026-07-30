// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/bnpl/coordinator/autofill_bnpl_mediator.h"

#import "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/settings/autofill/bnpl/ui/autofill_bnpl_consumer.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

class AutofillBnplMediatorTest : public PlatformTest {
 protected:
  web::WebTaskEnvironment task_environment_;
  AutofillBnplMediatorTest() {
    prefs_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillBnplEnabled, false);

    mediator_ = [[AutofillBnplMediator alloc]
        initWithPersonalDataManager:&personal_data_manager_
                        prefService:&prefs_];

    consumer_mock_ = OCMProtocolMock(@protocol(AutofillBnplConsumer));
  }

  void TearDown() override { [mediator_ disconnect]; }

  autofill::TestPersonalDataManager personal_data_manager_;
  TestingPrefServiceSimple prefs_;
  AutofillBnplMediator* mediator_;
  id consumer_mock_;
};

// Tests that the consumer is updated with the preference value when set.
TEST_F(AutofillBnplMediatorTest, TestConsumerInitialization) {
  prefs_.SetBoolean(autofill::prefs::kAutofillBnplEnabled, true);

  OCMExpect([consumer_mock_ setBnplSwitchIsOn:YES]);

  mediator_.consumer = consumer_mock_;

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}

// Tests that the preference is updated when the switch is changed in the view
// controller.
TEST_F(AutofillBnplMediatorTest, TestSwitchChanged) {
  mediator_.consumer = consumer_mock_;

  EXPECT_FALSE(prefs_.GetBoolean(autofill::prefs::kAutofillBnplEnabled));

  [mediator_ viewController:nil didChangeBnplSwitchTo:YES];

  EXPECT_TRUE(prefs_.GetBoolean(autofill::prefs::kAutofillBnplEnabled));
}

// Tests that the consumer is updated when the preference changes from outside.
TEST_F(AutofillBnplMediatorTest, TestPrefChanged) {
  mediator_.consumer = consumer_mock_;

  OCMExpect([consumer_mock_ setBnplSwitchIsOn:YES]);

  prefs_.SetBoolean(autofill::prefs::kAutofillBnplEnabled, true);

  EXPECT_OCMOCK_VERIFY(consumer_mock_);
}
