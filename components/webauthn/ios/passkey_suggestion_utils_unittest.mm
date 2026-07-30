// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/webauthn/ios/passkey_suggestion_utils.h"

#import "base/base64.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/strings/grit/components_strings.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util.h"

using autofill::SuggestionType;
using PasskeySuggestionUtilsTest = PlatformTest;
using password_manager::PasskeyCredential;

namespace webauthn {

namespace {

constexpr std::string kDisplayName = "display name";
constexpr NSString* kPasskeySuggestionValue = @"passkey";
constexpr NSString* kPasswordSuggestionValue = @"password";
constexpr std::string kRpId = "example.com";
constexpr std::string kUsername = "username";

// Returns a generic PasskeyCredential with the provided `display_name`.
PasskeyCredential CreatePasskeyCredential(
    std::string display_name = std::string()) {
  PasskeyCredential passkey_credential(
      PasskeyCredential::Source::kGooglePasswordManager,
      PasskeyCredential::RpId(kRpId),
      PasskeyCredential::CredentialId({1, 2, 3, 4}),
      PasskeyCredential::UserId(), PasskeyCredential::Username(kUsername),
      PasskeyCredential::DisplayName(display_name));
  return passkey_credential;
}

// Returns a generic FormSuggestion of type `type`.
FormSuggestion* CreateFormSuggestion(NSString* value, SuggestionType type) {
  return [FormSuggestion suggestionWithValue:value
                          displayDescription:nil
                                        icon:nil
                                        type:type
                                     payload:autofill::Suggestion::Payload()
                              requiresReauth:NO];
}

}  // namespace

// Tests that PasskeyCredentials are correctly converted to FormSuggestions.
TEST_F(PasskeySuggestionUtilsTest, FormSuggestionsFromPasskeyCredentials) {
  std::vector<password_manager::PasskeyCredential> passkeys = {
      CreatePasskeyCredential()};

  NSArray<FormSuggestion*>* suggestions =
      FormSuggestionsFromPasskeyCredentials(passkeys);

  ASSERT_EQ(1u, suggestions.count);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kUsername), suggestions[0].value);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL),
              suggestions[0].displayDescription);
  ASSERT_TRUE(std::holds_alternative<autofill::Suggestion::Guid>(
      suggestions[0].payload));
  EXPECT_EQ(
      std::get<autofill::Suggestion::Guid>(suggestions[0].payload).value(),
      base::Base64Encode(passkeys[0].credential_id()));
  EXPECT_NSEQ(suggestions[0].minorValue, base::SysUTF8ToNSString(kRpId));
}

// Tests that converting a PasskeyCredential with a display name results in a
// FormSuggestion with the expected `value` and `displayDescription`.
TEST_F(PasskeySuggestionUtilsTest,
       FormSuggestionFromPasskeyCredentialWithDisplayName) {
  NSArray<FormSuggestion*>* suggestions = FormSuggestionsFromPasskeyCredentials(
      {CreatePasskeyCredential(/*display_name=*/kDisplayName)});

  // The passkey's username should be displayed in the display description as
  // the passkey's display name will appear as the value of the suggestion.
  NSString* expected_display_description = [NSString
      stringWithFormat:@"%@ • %@",
                       l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL),
                       base::SysUTF8ToNSString(kUsername)];

  ASSERT_EQ(1u, suggestions.count);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kDisplayName), suggestions[0].value);
  EXPECT_NSEQ(expected_display_description, suggestions[0].displayDescription);
}

// Tests that passkey and password suggestion are merged into a single array as
// expected.
TEST_F(PasskeySuggestionUtilsTest, MergePasskeyAndPasswordSuggestions) {
  NSArray<FormSuggestion*>* passkey_suggestions = @[ CreateFormSuggestion(
      kPasskeySuggestionValue, SuggestionType::kWebauthnCredential) ];
  NSArray<FormSuggestion*>* password_suggestions = @[ CreateFormSuggestion(
      kPasswordSuggestionValue, SuggestionType::kPasswordEntry) ];

  NSArray<FormSuggestion*>* merged = MergePasskeyAndPasswordSuggestions(
      passkey_suggestions, password_suggestions);

  ASSERT_EQ(2u, merged.count);
  EXPECT_NSEQ(kPasskeySuggestionValue, merged[0].value);
  EXPECT_NSEQ(kPasswordSuggestionValue, merged[1].value);
}

// Tests that converting a PasskeyCredential with a display name equal to its
// username results in a FormSuggestion with only the Passkey label as its
// `displayDescription`.
TEST_F(PasskeySuggestionUtilsTest, SameDisplayNameAndUsername) {
  NSArray<FormSuggestion*>* suggestions = FormSuggestionsFromPasskeyCredentials(
      {CreatePasskeyCredential(/*display_name=*/kUsername)});

  ASSERT_EQ(1u, suggestions.count);
  EXPECT_NSEQ(base::SysUTF8ToNSString(kUsername), suggestions[0].value);
  EXPECT_NSEQ(l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL),
              suggestions[0].displayDescription);
}

// Tests the formatting logic for a passkey Manual Fill cell's subtitle.
TEST_F(PasskeySuggestionUtilsTest, ManualFillSubtitleFormat) {
  NSString* passkey_label =
      l10n_util::GetNSString(IDS_IOS_PASSKEY_SUGGESTION_LABEL);
  NSString* expected_subtitle =
      [NSString stringWithFormat:@"%@ • %@", passkey_label,
                                 base::SysUTF8ToNSString(kDisplayName)];

  EXPECT_NSEQ(passkey_label,
              FormatPasskeyManualFillSubtitle(/*display_name=*/nil));
  EXPECT_NSEQ(passkey_label,
              FormatPasskeyManualFillSubtitle(/*display_name=*/@""));
  EXPECT_NSEQ(expected_subtitle, FormatPasskeyManualFillSubtitle(
                                     base::SysUTF8ToNSString(kDisplayName)));
}

}  // namespace webauthn
