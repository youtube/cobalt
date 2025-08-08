// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "base/test/scoped_feature_list.h"
#import "components/password_manager/core/common/password_manager_pref_names.h"
#import "components/prefs/pref_registry_simple.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/credential_provider_promo/model/features.h"
#import "ios/chrome/browser/promos_manager/mock_promos_manager.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/public/commands/credential_provider_promo_commands.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_constants.h"
#import "ios/chrome/browser/ui/credential_provider_promo/credential_provider_promo_consumer.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/branded_images/branded_images_api.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "ui/base/l10n/l10n_util.h"

namespace {
NSString* const kFirstStepAnimation = @"CPE_promo_animation_edu_autofill";
NSString* const kLearnMoreAnimation = @"CPE_promo_animation_edu_how_to_enable";
UIImage* kImage = ios::provider::GetBrandedImage(
    ios::provider::BrandedImage::kPasswordSuggestionKey);

NSString* LearnMoreSubtitleString() {
  if (@available(iOS 16, *)) {
    return l10n_util::GetNSStringF(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_SUBTITLE_WITH_PH,
        base::SysNSStringToUTF16(l10n_util::GetNSString(
            IDS_IOS_CREDENTIAL_PROVIDER_PROMO_OS_PASSWORDS_SETTINGS_TITLE_IOS16)));
  } else {
    return l10n_util::GetNSStringF(
        IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_SUBTITLE_WITH_PH,
        base::SysNSStringToUTF16(l10n_util::GetNSString(
            IDS_IOS_CREDENTIAL_PROVIDER_PROMO_OS_PASSWORDS_SETTINGS_TITLE_BELOW_IOS16)));
  }
}

}  // namespace

// Test fixture for testing the CredentialProviderPromoMediator class.
class CredentialProviderPromoMediatorTest : public PlatformTest {
 protected:
  CredentialProviderPromoMediatorTest() {
    CreateCredentialProviderPromoMediator();
    DisableCredentialProviderExtension();
    EnableCredentialProviderPromoFlag();
  }

 protected:
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<MockPromosManager> promos_manager_;
  CredentialProviderPromoMediator* mediator_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
  id consumer_;
  NSString* firstStepTitleString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_TITLE);
  NSString* firstStepSubtitleString = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_PROMO_INITIAL_SUBTITLE);
  NSString* learnMoreTitleString = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_MORE_TITLE);
  NSString* goToSettingsString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_GO_TO_SETTINGS);
  NSString* learnHowString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_LEARN_HOW);
  NSString* noThanksString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_NO_THANKS);
  NSString* remindMeLaterString =
      l10n_util::GetNSString(IDS_IOS_CREDENTIAL_PROVIDER_PROMO_REMIND_ME_LATER);

  void CreateCredentialProviderPromoMediator() {
    promos_manager_ = std::make_unique<MockPromosManager>();
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    consumer_ = OCMProtocolMock(@protocol(CredentialProviderPromoConsumer));
    mediator_ = [[CredentialProviderPromoMediator alloc]
        initWithPromosManager:promos_manager_.get()
                  prefService:pref_service_.get()];
    mediator_.consumer = consumer_;
  }

  void EnableCredentialProviderPromoFlag() {
    scoped_feature_list_.InitWithFeatures({kCredentialProviderExtensionPromo},
                                          {});
  }

  void DisableCredentialProviderExtension() {
    pref_service_->registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialProviderEnabledOnStartup, false);
  }

  void ExpectConsumerSetFieldsForFirstStep() {
    OCMExpect([consumer_ setTitleString:firstStepTitleString
                         subtitleString:firstStepSubtitleString
                    primaryActionString:learnHowString
                  secondaryActionString:noThanksString
                   tertiaryActionString:remindMeLaterString
                                  image:nil]);
    OCMExpect([consumer_ setAnimation:kFirstStepAnimation]);
  }

  void ExpectConsumerSetFieldsForFirstStepNoAnimation() {
    OCMExpect([consumer_ setTitleString:firstStepTitleString
                         subtitleString:firstStepSubtitleString
                    primaryActionString:learnHowString
                  secondaryActionString:noThanksString
                   tertiaryActionString:remindMeLaterString
                                  image:kImage]);
  }

  void ExpectConsumerSetFieldsForLearnMore() {
    OCMExpect([consumer_ setTitleString:learnMoreTitleString
                         subtitleString:LearnMoreSubtitleString()
                    primaryActionString:goToSettingsString
                  secondaryActionString:noThanksString
                   tertiaryActionString:remindMeLaterString
                                  image:nil]);
    OCMExpect([consumer_ setAnimation:kLearnMoreAnimation]);
  }
};

#pragma mark - Tests

// Tests that promo is NOT displayed when the user has already enabled the
// Credential Provider Extension.
TEST_F(CredentialProviderPromoMediatorTest,
       CredentialProviderExtensionEnabled) {
  pref_service_->SetBoolean(
      password_manager::prefs::kCredentialProviderEnabledOnStartup, true);

  EXPECT_FALSE([mediator_ canShowCredentialProviderPromoWithTrigger:
                              CredentialProviderPromoTrigger::PasswordCopied
                                                          promoSeen:NO]);
}

// Tests that promo will NOT be displayed when the Credential Provider Promo
// feature flag is not enabled.
TEST_F(CredentialProviderPromoMediatorTest, CredentialProviderPromoNotEnabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(kCredentialProviderExtensionPromo);

  EXPECT_FALSE([mediator_ canShowCredentialProviderPromoWithTrigger:
                              CredentialProviderPromoTrigger::PasswordCopied
                                                          promoSeen:NO]);
}

// Tests that promo will NOT be displayed when the promo has already been
// displayed in the current app session.
TEST_F(CredentialProviderPromoMediatorTest,
       CredentialProviderPromoAlreadySeen) {
  EXPECT_FALSE([mediator_ canShowCredentialProviderPromoWithTrigger:
                              CredentialProviderPromoTrigger::PasswordCopied
                                                          promoSeen:YES]);
}

// Tests that promo will NOT be displayed when the user has previously seen the
// promo and selected "No Thanks".
TEST_F(CredentialProviderPromoMediatorTest,
       CredentialProviderPromoNoThanksSelected) {
  local_state_.Get()->SetBoolean(prefs::kIosCredentialProviderPromoStopPromo,
                                 true);

  EXPECT_FALSE([mediator_ canShowCredentialProviderPromoWithTrigger:
                              CredentialProviderPromoTrigger::PasswordCopied
                                                          promoSeen:NO]);
}

// Tests that the promo will be displayed when all the trigger requirements are
// met.
TEST_F(CredentialProviderPromoMediatorTest,
       CredentialProviderPromoRequirementsMet) {
  EXPECT_TRUE([mediator_ canShowCredentialProviderPromoWithTrigger:
                             CredentialProviderPromoTrigger::PasswordCopied
                                                         promoSeen:NO]);
}

// Tests that the promo will always be displayed when the trigger is SetUpList.
TEST_F(CredentialProviderPromoMediatorTest,
       CredentialProviderPromoSetUpListTrigger) {
  local_state_.Get()->SetBoolean(prefs::kIosCredentialProviderPromoStopPromo,
                                 true);
  EXPECT_TRUE([mediator_ canShowCredentialProviderPromoWithTrigger:
                             CredentialProviderPromoTrigger::SetUpList
                                                         promoSeen:YES]);
}

// Tests that the consumer content is correctly set when the promo:
// - Is a “First Step” promo
// - Was triggered by the user copying a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_FirstStep_PasswordCopied) {
  ExpectConsumerSetFieldsForFirstStep();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       PasswordCopied
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the promo:
//  - Is a “First Step” promo
//  - Was triggered by the user saving a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_FirstStep_PasswordSaved) {
  ExpectConsumerSetFieldsForFirstStepNoAnimation();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::PasswordSaved
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the promo:
// - Is a “First Step” promo
// - Was triggered by the user successfully logging in using an existing
// password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_FirstStep_SuccessfulLoginUsingExistingPassword) {
  ExpectConsumerSetFieldsForFirstStepNoAnimation();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       SuccessfulLoginUsingExistingPassword
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” AFTER seeing a promo that:
// - Was a “First Step” promo
// - Was triggered by the user copying a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_FirstStep_RemindMeLater_AfterFirstStepPasswordCopied) {
  // Need to check for each of these method calls twice: once for the promo
  // triggered by copying password and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForFirstStep();
  ExpectConsumerSetFieldsForFirstStep();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       PasswordCopied
                           context:CredentialProviderPromoContext::kFirstStep];

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” AFTER seeing a promo that:
// - Was a “First Step” promo
// - Was triggered by the user saving a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_FirstStep_RemindMeLater_AfterFirstStepPasswordSaved) {
  // Need to check for this method call twice: once for the promo
  // triggered by saving password and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForFirstStepNoAnimation();
  ExpectConsumerSetFieldsForFirstStepNoAnimation();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::PasswordSaved
                           context:CredentialProviderPromoContext::kFirstStep];
  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” AFTER seeing a promo that:
//  - Was a “First Step” promo
//  - Was triggered by the user successfully logging in using an existing
//  password
TEST_F(
    CredentialProviderPromoMediatorTest,
    ConsumerContent_FirstStep_RemindMeLater_AfterFirstStepSuccessfulLoginUsingExistingPassword) {
  // Need to check for this method call twice: once for the promo
  // triggered by successful login and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForFirstStepNoAnimation();
  ExpectConsumerSetFieldsForFirstStepNoAnimation();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       SuccessfulLoginUsingExistingPassword
                           context:CredentialProviderPromoContext::kFirstStep];
  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kFirstStep];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the promo:
//  - Is a “Learn More” promo
//  - Was triggered by the user copying a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_LearnMore_PasswordCopied) {
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       PasswordCopied
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the promo:
//  - Is a “Learn More” promo
//  - Was triggered by the user saving a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_LearnMore_PasswordSaved) {
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::PasswordSaved
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the promo:
//  - Is a “Learn More” promo
//  - Was triggered by the user successfully logging in using an existing
//  password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_LearnMore_SuccessfulLoginUsingExistingPassword) {
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       SuccessfulLoginUsingExistingPassword
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” after seeing a promo that:
//  - Was a “Learn More” promo
//  - Was triggered by the user copying a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_LearnMore_RemindMeLater_AfterPasswordCopied) {
  // Need to check for each of these method calls twice: once for the promo
  // triggered by copying password and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForLearnMore();
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       PasswordCopied
                           context:CredentialProviderPromoContext::kLearnMore];
  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” after seeing a promo that:
//  - Was a “Learn More” promo
//  - Was triggered by the user saving a password
TEST_F(CredentialProviderPromoMediatorTest,
       ConsumerContent_LearnMore_RemindMeLater_AfterPasswordSaved) {
  // Need to check for each of these method calls twice: once for the promo
  // triggered by saving password and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForLearnMore();
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::PasswordSaved
                           context:CredentialProviderPromoContext::kLearnMore];
  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}

// Tests that the consumer content is correctly set when the user selects
// “Remind Me Later” after seeing a promo that:
//  - Was a “Learn More” promo
//  - Was triggered by the user successfully logging in using an existing
//  password
TEST_F(
    CredentialProviderPromoMediatorTest,
    ConsumerContent_LearnMore_RemindMeLater_AfterSuccessfulLoginUsingExistingPassword) {
  // Need to check for each of these method calls twice: once for the promo
  // triggered by successful login and once for the promo triggered by selecting
  // "Remind Me Later".
  ExpectConsumerSetFieldsForLearnMore();
  ExpectConsumerSetFieldsForLearnMore();

  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::
                                       SuccessfulLoginUsingExistingPassword
                           context:CredentialProviderPromoContext::kLearnMore];
  [mediator_
      configureConsumerWithTrigger:CredentialProviderPromoTrigger::RemindMeLater
                           context:CredentialProviderPromoContext::kLearnMore];

  EXPECT_OCMOCK_VERIFY(consumer_);
}
