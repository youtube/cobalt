// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/ios/account_select_fill_data.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/password_manager/ios/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using autofill::FieldRendererId;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::UsernameAndRealm;
using test_helpers::SetPasswordFormFillData;

namespace {
// Test data.
const char* kUrl = "http://example.com/";
const char* kFormNames[] = {"form_name1", "form_name2"};
const uint32_t kFormUniqueIDs[] = {0, 3};
const char* kUsernameElements[] = {"username1", "username2"};
const uint32_t kUsernameUniqueIDs[] = {1, 4};
const char* kUsernames[] = {"user0", "u_s_e_r"};
const char* kPasswordElements[] = {"password1", "password2"};
const uint32_t kPasswordUniqueIDs[] = {2, 5};
const char* kPasswords[] = {"password0", "secret"};
const char* kAdditionalUsernames[] = {"u$er2", nullptr};
const char* kAdditionalPasswords[] = {"secret", nullptr};

class AccountSelectFillDataTest : public PlatformTest {
 public:
  AccountSelectFillDataTest() {
    for (size_t i = 0; i < std::size(form_data_); ++i) {
      SetPasswordFormFillData(
          kUrl, kFormNames[i], kFormUniqueIDs[i], kUsernameElements[i],
          kUsernameUniqueIDs[i], kUsernames[i], kPasswordElements[i],
          kPasswordUniqueIDs[i], kPasswords[i], kAdditionalUsernames[i],
          kAdditionalPasswords[i], &form_data_[i]);
    }
  }

 protected:
  PasswordFormFillData form_data_[2];
};

TEST_F(AccountSelectFillDataTest, EmptyReset) {
  AccountSelectFillData account_select_fill_data;
  EXPECT_TRUE(account_select_fill_data.Empty());

  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  EXPECT_FALSE(account_select_fill_data.Empty());

  account_select_fill_data.Reset();
  EXPECT_TRUE(account_select_fill_data.Empty());
}

TEST_F(AccountSelectFillDataTest, IsSuggestionsAvailableOneForm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);

  // Suggestions are available for the correct form and field ids.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false));
  // Suggestion should be available to any password field.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id, FieldRendererId(404), true));

  // Suggestions are not available for different form renderer_id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].username_element_renderer_id, false));
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].password_element_renderer_id, true));

  // Suggestions are not available for different field id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id, FieldRendererId(404), false));
}

TEST_F(AccountSelectFillDataTest, IsSuggestionsAvailableTwoForms) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  account_select_fill_data.Add(form_data_[1], /*is_cross_origin_iframe=*/false);

  // Suggestions are available for the correct form and field names.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false));
  // Suggestions are available for the correct form and field names.
  EXPECT_TRUE(account_select_fill_data.IsSuggestionsAvailable(
      form_data_[1].form_renderer_id,
      form_data_[1].username_element_renderer_id, false));
  // Suggestions are not available for different form id.
  EXPECT_FALSE(account_select_fill_data.IsSuggestionsAvailable(
      FormRendererId(404), form_data_[0].username_element_renderer_id, false));
}

TEST_F(AccountSelectFillDataTest, RetrieveSuggestionsOneForm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);

  for (bool is_password_field : {false, true}) {
    const FieldRendererId field_id =
        is_password_field ? form_data_[0].password_element_renderer_id
                          : form_data_[0].username_element_renderer_id;
    std::vector<UsernameAndRealm> suggestions =
        account_select_fill_data.RetrieveSuggestions(
            form_data_[0].form_renderer_id, field_id, is_password_field);
    EXPECT_EQ(2u, suggestions.size());
    EXPECT_EQ(base::ASCIIToUTF16(kUsernames[0]), suggestions[0].username);
    EXPECT_EQ(std::string(), suggestions[0].realm);
    EXPECT_EQ(base::ASCIIToUTF16(kAdditionalUsernames[0]),
              suggestions[1].username);
    EXPECT_EQ(std::string(), suggestions[1].realm);
  }
}

TEST_F(AccountSelectFillDataTest, RetrieveSuggestionsTwoForm) {
  // Test that after adding two PasswordFormFillData, suggestions for both forms
  // are shown, with credentials from the second PasswordFormFillData. That
  // emulates the case when credentials in the Password Store were changed
  // between load the first and the second forms.
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  account_select_fill_data.Add(form_data_[1], /*is_cross_origin_iframe=*/false);

  std::vector<UsernameAndRealm> suggestions =
      account_select_fill_data.RetrieveSuggestions(
          form_data_[0].form_renderer_id,
          form_data_[0].username_element_renderer_id, false);
  EXPECT_EQ(1u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), suggestions[0].username);

  suggestions = account_select_fill_data.RetrieveSuggestions(
      form_data_[1].form_renderer_id,
      form_data_[1].username_element_renderer_id, false);
  EXPECT_EQ(1u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), suggestions[0].username);
}

TEST_F(AccountSelectFillDataTest, RetrievePSLMatchedSuggestions) {
  AccountSelectFillData account_select_fill_data;
  const char* kRealm = "http://a.example.com/";
  std::string kAdditionalRealm = "http://b.example.com/";

  // Make logins to be PSL matched by setting non-empy realm.
  form_data_[0].preferred_login.realm = kRealm;
  form_data_[0].additional_logins.begin()->realm = kAdditionalRealm;

  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  std::vector<UsernameAndRealm> suggestions =
      account_select_fill_data.RetrieveSuggestions(
          form_data_[0].form_renderer_id,
          form_data_[0].username_element_renderer_id, false);
  EXPECT_EQ(2u, suggestions.size());
  EXPECT_EQ(base::ASCIIToUTF16(kUsernames[0]), suggestions[0].username);
  EXPECT_EQ(kRealm, suggestions[0].realm);
  EXPECT_EQ(base::ASCIIToUTF16(kAdditionalUsernames[0]),
            suggestions[1].username);
  EXPECT_EQ(kAdditionalRealm, suggestions[1].realm);
}

TEST_F(AccountSelectFillDataTest, GetFillData) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  account_select_fill_data.Add(form_data_[1], /*is_cross_origin_iframe=*/false);

  for (bool is_password_field : {false, true}) {
    for (size_t form_i = 0; form_i < std::size(form_data_); ++form_i) {
      const auto& form_data = form_data_[form_i];
      // Suggestions should be shown on any password field on the form. So in
      // case of clicking on a password field it is taken an id different from
      // existing field ids.
      const FieldRendererId password_field_id =
          is_password_field ? FieldRendererId(1000)
                            : form_data.password_element_renderer_id;
      const FieldRendererId clicked_field =
          is_password_field ? password_field_id
                            : form_data.username_element_renderer_id;

      // GetFillData() doesn't have form identifier in arguments, it should be
      // provided in RetrieveSuggestions().
      account_select_fill_data.RetrieveSuggestions(
          form_data.form_renderer_id, clicked_field, is_password_field);
      std::unique_ptr<FillData> fill_data =
          account_select_fill_data.GetFillData(
              base::ASCIIToUTF16(kUsernames[1]));

      ASSERT_TRUE(fill_data);
      EXPECT_EQ(form_data.url, fill_data->origin);
      EXPECT_EQ(form_data.form_renderer_id.value(), fill_data->form_id.value());
      EXPECT_EQ(kUsernameUniqueIDs[form_i],
                fill_data->username_element_id.value());
      EXPECT_EQ(base::ASCIIToUTF16(kUsernames[1]), fill_data->username_value);
      EXPECT_EQ(password_field_id, fill_data->password_element_id);
      EXPECT_EQ(base::ASCIIToUTF16(kPasswords[1]), fill_data->password_value);
    }
  }
}

TEST_F(AccountSelectFillDataTest, GetFillDataOldCredentials) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/false);
  account_select_fill_data.Add(form_data_[1], /*is_cross_origin_iframe=*/false);

  // GetFillData() doesn't have form identifier in arguments, it should be
  // provided in RetrieveSuggestions().
  account_select_fill_data.RetrieveSuggestions(
      form_data_[0].form_renderer_id,
      form_data_[0].username_element_renderer_id, false);

  // AccountSelectFillData should keep only last credentials. Check that in
  // request of old credentials nothing is returned.
  std::unique_ptr<FillData> fill_data =
      account_select_fill_data.GetFillData(base::ASCIIToUTF16(kUsernames[0]));
  EXPECT_FALSE(fill_data);
}

TEST_F(AccountSelectFillDataTest, CrossOriginSuggestionHasRealm) {
  AccountSelectFillData account_select_fill_data;
  account_select_fill_data.Add(form_data_[0], /*is_cross_origin_iframe=*/true);

  for (bool is_password_field : {false, true}) {
    const FieldRendererId field_id =
        is_password_field ? form_data_[0].password_element_renderer_id
                          : form_data_[0].username_element_renderer_id;
    std::vector<UsernameAndRealm> suggestions =
        account_select_fill_data.RetrieveSuggestions(
            form_data_[0].form_renderer_id, field_id, is_password_field);
    EXPECT_EQ(2u, suggestions.size());
    EXPECT_EQ(kUrl, suggestions[0].realm);
    EXPECT_EQ(kUrl, suggestions[1].realm);
  }
}

}  // namespace
