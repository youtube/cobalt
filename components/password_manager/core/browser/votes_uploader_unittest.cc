// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/votes_uploader.h"

#include <string>
#include <utility>

#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/field_info_manager.h"
#include "components/password_manager/core/browser/field_info_store.h"
#include "components/password_manager/core/browser/field_info_table.h"
#include "components/password_manager/core/browser/mock_field_info_store.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/vote_uploads_test_matchers.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using autofill::AutofillDownloadManager;
using autofill::CONFIRMATION_PASSWORD;
using autofill::FieldRendererId;
using autofill::FieldSignature;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::FormSignature;
using autofill::FormStructure;
using autofill::NEW_PASSWORD;
using autofill::NOT_USERNAME;
using autofill::PASSWORD;
using autofill::PasswordAttribute;
using autofill::ServerFieldType;
using autofill::ServerFieldTypeSet;
using autofill::SignatureIsSameAs;
using autofill::SINGLE_USERNAME;
using autofill::SubmissionEventIsSameAs;
using autofill::UNKNOWN_TYPE;
using autofill::mojom::SubmissionIndicatorEvent;
using base::ASCIIToUTF16;
using testing::_;
using testing::AllOf;
using testing::AnyNumber;
using testing::IsNull;
using testing::Return;
using testing::SaveArg;

namespace password_manager {
namespace {

MATCHER_P3(FieldInfoHasData, form_signature, field_signature, field_type, "") {
  return arg.form_signature == form_signature &&
         arg.field_signature == field_signature &&
         arg.field_type == field_type && arg.create_time != base::Time();
}

constexpr int kNumberOfPasswordAttributes =
    static_cast<int>(PasswordAttribute::kPasswordAttributesCount);

constexpr FieldRendererId kSingleUsernameRendererId(101);
constexpr FieldSignature kSingleUsernameFieldSignature(1234);
constexpr FormSignature kSingleUsernameFormSignature(1000);

FormPredictions MakeSimpleSingleUsernamePredictions() {
  FormPredictions form_predictions;
  form_predictions.form_signature = kSingleUsernameFormSignature;
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kSingleUsernameRendererId;
  form_predictions.fields.back().signature = kSingleUsernameFieldSignature;
  return form_predictions;
}

autofill::AutofillUploadContents::SingleUsernameData
MakeSimpleSingleUsernameData() {
  autofill::AutofillUploadContents::SingleUsernameData single_username_data;
  single_username_data.set_username_form_signature(
      kSingleUsernameFormSignature.value());
  single_username_data.set_username_field_signature(
      kSingleUsernameFieldSignature.value());
  single_username_data.set_value_type(
      autofill::AutofillUploadContents::USERNAME_LIKE);
  return single_username_data;
}

class MockAutofillDownloadManager : public AutofillDownloadManager {
 public:
  MockAutofillDownloadManager()
      : AutofillDownloadManager(nullptr,
                                version_info::Channel::UNKNOWN,
                                nullptr) {}

  MockAutofillDownloadManager(const MockAutofillDownloadManager&) = delete;
  MockAutofillDownloadManager& operator=(const MockAutofillDownloadManager&) =
      delete;

  MOCK_METHOD(bool,
              StartUploadRequest,
              (const FormStructure&,
               bool,
               const ServerFieldTypeSet&,
               const std::string&,
               bool,
               PrefService*,
               base::WeakPtr<Observer>),
              (override));
};

class MockPasswordManagerClient : public StubPasswordManagerClient {
 public:
  MOCK_METHOD0(GetAutofillDownloadManager, AutofillDownloadManager*());
  MOCK_CONST_METHOD0(GetFieldInfoManager, FieldInfoManager*());
};

class MockFieldInfoManager : public FieldInfoManager {
 public:
  MOCK_METHOD(void,
              AddFieldType,
              (FormSignature, FieldSignature, ServerFieldType),
              (override));
  MOCK_METHOD(ServerFieldType,
              GetFieldType,
              (FormSignature, autofill::FieldSignature),
              (const override));
};

}  // namespace

class VotesUploaderTest : public testing::Test {
 public:
  VotesUploaderTest() {
    EXPECT_CALL(client_, GetAutofillDownloadManager())
        .WillRepeatedly(Return(&mock_autofill_download_manager_));

    ON_CALL(mock_autofill_download_manager_, StartUploadRequest)
        .WillByDefault(Return(true));

    // Create |fields| in |form_to_upload_| and |submitted_form_|. Only |name|
    // field in FormFieldData is important. Set them to the unique values based
    // on index.
    const size_t kNumberOfFields = 20;
    for (size_t i = 0; i < kNumberOfFields; ++i) {
      FormFieldData field;
      field.name = GetFieldNameByIndex(i);
      form_to_upload_.form_data.fields.push_back(field);
      submitted_form_.form_data.fields.push_back(field);
    }
    // Password attributes uploading requires a non-empty password value.
    form_to_upload_.password_value = u"password_value";
    submitted_form_.password_value = u"password_value";
  }

 protected:
  std::u16string GetFieldNameByIndex(size_t index) {
    return u"field" + base::NumberToString16(index);
  }

  base::test::TaskEnvironment task_environment_;
  MockAutofillDownloadManager mock_autofill_download_manager_;

  MockPasswordManagerClient client_;

  PasswordForm form_to_upload_;
  PasswordForm submitted_form_;

  std::string login_form_signature_ = "123";
};

TEST_F(VotesUploaderTest, UploadPasswordVoteUpdate) {
  VotesUploader votes_uploader(&client_, true);
  std::u16string new_password_element = GetFieldNameByIndex(3);
  std::u16string confirmation_element = GetFieldNameByIndex(11);
  form_to_upload_.new_password_element = new_password_element;
  submitted_form_.new_password_element = new_password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  form_to_upload_.new_password_value = u"new_password_value";
  submitted_form_.new_password_value = u"new_password_value";
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {NEW_PASSWORD,
                                             CONFIRMATION_PASSWORD};
  FieldTypeMap expected_types = {{new_password_element, NEW_PASSWORD},
                                 {confirmation_element, CONFIRMATION_PASSWORD}};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIsSameAs(form_to_upload_),
                        UploadedAutofillTypesAre(expected_types),
                        SubmissionEventIsSameAs(expected_submission_event)),
                  false, expected_field_types, login_form_signature_, true,
                  nullptr, /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, NEW_PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, UploadPasswordVoteSave) {
  VotesUploader votes_uploader(&client_, false);
  std::u16string password_element = GetFieldNameByIndex(5);
  std::u16string confirmation_element = GetFieldNameByIndex(12);
  form_to_upload_.password_element = password_element;
  submitted_form_.password_element = password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  submitted_form_.submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};
  SubmissionIndicatorEvent expected_submission_event =
      SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  SubmissionEventIsSameAs(expected_submission_event), false,
                  expected_field_types, login_form_signature_, true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));
}

TEST_F(VotesUploaderTest, InitialValueDetection) {
  // Tests if the initial value of the (predicted to be the) username field
  // in |form_data| is persistently stored and if it's low-entropy hash is
  // correctly written to the corresponding field in the |form_structure|.
  // Note that the value of the username field is deliberately altered before
  // the |form_structure| is generated from |form_data| to test the persistence.
  std::u16string prefilled_username = u"prefilled_username";
  FieldRendererId username_field_renderer_id(123456);
  const uint32_t kNumberOfHashValues = 64;
  FormData form_data;

  FormFieldData username_field;
  username_field.value = prefilled_username;
  username_field.unique_renderer_id = username_field_renderer_id;

  FormFieldData other_field;
  other_field.value = u"some_field";
  other_field.unique_renderer_id = FieldRendererId(3234);

  form_data.fields = {other_field, username_field};

  VotesUploader votes_uploader(&client_, true);
  votes_uploader.StoreInitialFieldValues(form_data);

  form_data.fields.at(1).value = u"user entered value";
  FormStructure form_structure(form_data);

  PasswordForm password_form;
  password_form.username_element_renderer_id = username_field_renderer_id;

  votes_uploader.SetInitialHashValueOfUsernameField(username_field_renderer_id,
                                                    &form_structure);

  const uint32_t expected_hash = 1377800651 % kNumberOfHashValues;

  int found_fields = 0;
  for (auto& f : form_structure) {
    if (f->unique_renderer_id == username_field_renderer_id) {
      found_fields++;
      ASSERT_TRUE(f->initial_value_hash());
      EXPECT_EQ(f->initial_value_hash().value(), expected_hash);
    }
  }
  EXPECT_EQ(found_fields, 1);
}

// Tests that password attributes are uploaded only if it is the first save or a
// password updated.
TEST_F(VotesUploaderTest, UploadPasswordAttributes) {
  for (const ServerFieldType autofill_type :
       {autofill::PASSWORD, autofill::ACCOUNT_CREATION_PASSWORD,
        autofill::NOT_ACCOUNT_CREATION_PASSWORD, autofill::NEW_PASSWORD,
        autofill::PROBABLY_NEW_PASSWORD, autofill::NOT_NEW_PASSWORD,
        autofill::USERNAME}) {
    SCOPED_TRACE(testing::Message() << "autofill_type=" << autofill_type);
    VotesUploader votes_uploader(&client_, false);
    if (autofill_type == autofill::NEW_PASSWORD ||
        autofill_type == autofill::PROBABLY_NEW_PASSWORD ||
        autofill_type == autofill::NOT_NEW_PASSWORD) {
      form_to_upload_.new_password_element = u"new_password_element";
      form_to_upload_.new_password_value = u"new_password_value";
    }

    bool expect_password_attributes = autofill_type == autofill::PASSWORD ||
                                      autofill_type == autofill::NEW_PASSWORD;
    EXPECT_CALL(mock_autofill_download_manager_,
                StartUploadRequest(
                    HasPasswordAttributesVote(expect_password_attributes),
                    false, _, login_form_signature_, true,
                    /* pref_service= */ nullptr,
                    /*observer=*/IsNull()));

    EXPECT_TRUE(votes_uploader.UploadPasswordVote(
        form_to_upload_, submitted_form_, autofill_type,
        login_form_signature_));

    testing::Mock::VerifyAndClearExpectations(&mock_autofill_download_manager_);
  }
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote) {
  VotesUploader votes_uploader(&client_, true);
  // Checks that randomization distorts information about present and missed
  // character classes, but a true value is still restorable with aggregation
  // of many distorted reports.
  const char* kPasswordSnippets[kNumberOfPasswordAttributes] = {"abc", "*-_"};
  for (int test_case = 0; test_case < 10; ++test_case) {
    bool has_password_attribute[kNumberOfPasswordAttributes];
    std::u16string password_value;
    for (int i = 0; i < kNumberOfPasswordAttributes; ++i) {
      has_password_attribute[i] = base::RandGenerator(2);
      if (has_password_attribute[i])
        password_value += ASCIIToUTF16(kPasswordSnippets[i]);
    }
    if (password_value.empty())
      continue;

    FormData form;
    FormStructure form_structure(form);
    int reported_false[kNumberOfPasswordAttributes] = {0, 0};
    int reported_true[kNumberOfPasswordAttributes] = {0, 0};

    int reported_actual_length = 0;
    int reported_wrong_length = 0;

    int kNumberOfRuns = 1000;

    for (int i = 0; i < kNumberOfRuns; ++i) {
      votes_uploader.GeneratePasswordAttributesVote(password_value,
                                                    &form_structure);
      absl::optional<std::pair<PasswordAttribute, bool>> vote =
          form_structure.get_password_attributes_vote();
      int attribute_index = static_cast<int>(vote->first);
      if (vote->second)
        reported_true[attribute_index]++;
      else
        reported_false[attribute_index]++;
      size_t reported_length = form_structure.get_password_length_vote();
      if (reported_length == password_value.size()) {
        reported_actual_length++;
      } else {
        reported_wrong_length++;
        EXPECT_LT(0u, reported_length);
        EXPECT_LT(reported_length, password_value.size());
      }
    }
    for (int i = 0; i < kNumberOfPasswordAttributes; i++) {
      EXPECT_LT(0, reported_false[i]);
      EXPECT_LT(0, reported_true[i]);

      // If the actual value is |true|, then it should report more |true|s than
      // |false|s.
      if (has_password_attribute[i]) {
        EXPECT_LT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      } else {
        EXPECT_GT(reported_false[i], reported_true[i])
            << "Wrong distribution for attribute " << i
            << ". password_value = " << password_value;
      }
    }
    EXPECT_LT(0, reported_actual_length);
    EXPECT_LT(0, reported_wrong_length);
    EXPECT_LT(reported_actual_length, reported_wrong_length);
  }
}

TEST_F(VotesUploaderTest, GeneratePasswordSpecialSymbolVote) {
  VotesUploader votes_uploader(&client_, true);

  const std::u16string password_value = u"password-withsymbols!";
  const int kNumberOfRuns = 2000;
  const int kSpecialSymbolsAttribute =
      static_cast<int>(PasswordAttribute::kHasSpecialSymbol);

  FormData form;

  int correct_symbol_reported = 0;
  int wrong_symbol_reported = 0;
  int number_of_symbol_votes = 0;

  for (int i = 0; i < kNumberOfRuns; ++i) {
    FormStructure form_structure(form);

    votes_uploader.GeneratePasswordAttributesVote(password_value,
                                                  &form_structure);
    absl::optional<std::pair<PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote();

    // Continue if the vote is not about special symbols or implies that no
    // special symbols are used.
    if (static_cast<int>(vote->first) != kSpecialSymbolsAttribute ||
        !vote->second) {
      EXPECT_EQ(form_structure.get_password_symbol_vote(), 0);
      continue;
    }

    number_of_symbol_votes += 1;

    int symbol = form_structure.get_password_symbol_vote();
    if (symbol == '-' || symbol == '!')
      correct_symbol_reported += 1;
    else
      wrong_symbol_reported += 1;
  }
  EXPECT_LT(0.4 * number_of_symbol_votes, correct_symbol_reported);
  EXPECT_LT(0.15 * number_of_symbol_votes, wrong_symbol_reported);
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_OneCharacterPassword) {
  // |VotesUploader::GeneratePasswordAttributesVote| shouldn't crash if a
  // password has only one character.
  FormData form;
  FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(u"1", &form_structure);
  absl::optional<std::pair<PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote();
  EXPECT_TRUE(vote.has_value());
  size_t reported_length = form_structure.get_password_length_vote();
  EXPECT_EQ(1u, reported_length);
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_AllAsciiCharacters) {
  FormData form;
  FormStructure form_structure(form);
  VotesUploader votes_uploader(&client_, true);
  votes_uploader.GeneratePasswordAttributesVote(
      u"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqr"
      u"stuvwxyz!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~",
      &form_structure);
  absl::optional<std::pair<PasswordAttribute, bool>> vote =
      form_structure.get_password_attributes_vote();
  EXPECT_TRUE(vote.has_value());
}

TEST_F(VotesUploaderTest, GeneratePasswordAttributesVote_NonAsciiPassword) {
  // Checks that password attributes vote is not generated if the password has
  // non-ascii characters.
  for (const auto* password :
       {u"пароль1", u"パスワード", u"münchen", u"סיסמה-A", u"Σ-12345",
        u"գաղտնաբառըTTT", u"Slaptažodis", u"密碼", u"كلمهالسر", u"mậtkhẩu!",
        u"ລະຫັດຜ່ານ-l", u"စကားဝှက်ကို3", u"პაროლი", u"पारण शब्द"}) {
    FormData form;
    FormStructure form_structure(form);
    VotesUploader votes_uploader(&client_, true);
    votes_uploader.GeneratePasswordAttributesVote(password, &form_structure);
    absl::optional<std::pair<PasswordAttribute, bool>> vote =
        form_structure.get_password_attributes_vote();

    EXPECT_FALSE(vote.has_value()) << password;
  }
}

TEST_F(VotesUploaderTest, NoSingleUsernameDataNoUpload) {
  VotesUploader votes_uploader(&client_, false);
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
  votes_uploader.MaybeSendSingleUsernameVote();
}

TEST_F(VotesUploaderTest, UploadSingleUsernameMultipleFieldsInUsernameForm) {
  VotesUploader votes_uploader(&client_, false);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType(_, _))
      .WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager())
      .WillByDefault(Return(&mock_field_manager));

  // Make form predictions for a form with multiple fields.
  FormPredictions form_predictions;
  form_predictions.form_signature = kSingleUsernameFormSignature;
  // Add a non-username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id.value() =
      kSingleUsernameRendererId.value() - 1;
  form_predictions.fields.back().signature.value() =
      kSingleUsernameFieldSignature.value() - 1;
  // Add the username field.
  form_predictions.fields.emplace_back();
  form_predictions.fields.back().renderer_id = kSingleUsernameRendererId;
  form_predictions.fields.back().signature = kSingleUsernameFieldSignature;

  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      form_predictions,
      /*stored_credentials=*/{}, /*password_form_had_username_field=*/false);
  votes_uploader.set_suggested_username(single_username_candidate_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  false, expected_types, std::string(), true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();
}

// Tests that a negeative vote is sent if the username candidate field
// value contained whitespaces.
TEST_F(VotesUploaderTest, UploadNotSingleUsernameForWhitespaces) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  VotesUploader votes_uploader(&client_, false);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType(_, _))
      .WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager())
      .WillByDefault(Return(&mock_field_manager));

  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId,
      /*username_candidate_value=*/u"some search query",
      MakeSimpleSingleUsernamePredictions(),
      /*stored_credentials=*/{}, /*password_form_had_username_field=*/false);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/u"saved_value");
#endif  // !BUILDFLAG(IS_ANDROID)

  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  false, expected_types, std::string(), true,
                  /* pref_service= */ nullptr, /*observer=*/IsNull()));

  votes_uploader.MaybeSendSingleUsernameVote();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::VALUE_WITH_WHITESPACE);
#if !BUILDFLAG(IS_ANDROID)
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_NEGATIVE);
#else
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));

  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that SINGLE_USERNAME vote and NOT_EDITED_IN_PROMPT vote type
// are sent if single username candidate value was suggested and accepted.
TEST_F(VotesUploaderTest, SingleUsernameValueSuggestedAndAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
  votes_uploader.set_suggested_username(single_username_candidate_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
#if !BUILDFLAG(IS_ANDROID)
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_POSITIVE);
#else
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));

  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote and NOT_EDITED_IN_PROMPT vote type
// are sent if value other than single username candidate was suggested and
// accepted.
TEST_F(VotesUploaderTest, SingleUsernameOtherValueSuggestedAndAccepted) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
  std::u16string suggested_value = u"other_value";
  votes_uploader.set_suggested_username(suggested_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/suggested_value);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::WEAK)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
#if !BUILDFLAG(IS_ANDROID)
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::NOT_EDITED_NEGATIVE);
#else
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that SINGLE_USERNAME vote and EDITED_IN_PROMPT vote type are sent
// if value other than single username candidate was suggested, but the user
// has inputted single username candidate value in prompt.
TEST_F(VotesUploaderTest, SingleUsernameValueSetInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
  std::u16string suggested_value = u"other_value";
  votes_uploader.set_suggested_username(suggested_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  ServerFieldTypeSet expected_types = {SINGLE_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();

  // Upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
#if !BUILDFLAG(IS_ANDROID)
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_POSITIVE);
#else
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote and EDITED_IN_PROMPT vote type are sent
// if single username candidate value was suggested, but the user has deleted
// it in prompt.
TEST_F(VotesUploaderTest, SingleUsernameValueDeletedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
  votes_uploader.set_suggested_username(single_username_candidate_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(/*saved_username=*/u"");
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  // Upload on the username form.
  ServerFieldTypeSet expected_types = {NOT_USERNAME};
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(
                  AllOf(SignatureIs(kSingleUsernameFormSignature),
                        UploadedSingleUsernameVoteTypeIs(
                            autofill::AutofillUploadContents::Field::STRONG)),
                  /*form_was_autofilled=*/false, expected_types,
                  /*login_form_signature=*/"",
                  /*observed_submission=*/true,
                  /*pref_service=*/nullptr,
                  /*observer=*/IsNull()));
#else
  EXPECT_CALL(mock_autofill_download_manager_, StartUploadRequest).Times(0);
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();

  // Expect upload for the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
#if !BUILDFLAG(IS_ANDROID)
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDITED_NEGATIVE);
#else
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
#endif  // !BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that no vote is sent if the user has deleted the username value
// suggested in prompt, and suggested value wasn't equal to single username
// candidate value.
TEST_F(VotesUploaderTest, NotSingleUsernameValueDeletedInPrompt) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
  std::u16string other_value = u"other_value";
  votes_uploader.set_suggested_username(other_value);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(/*saved_username=*/u"");
#endif  // !BUILDFLAG(IS_ANDROID)

  // Expect no upload on username form, as th signal is not informative to us.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature), _,
                                 _, _, _, _, /*observer=*/IsNull()))
      .Times(0);
  votes_uploader.MaybeSendSingleUsernameVote();

  // Expect upload for the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data = MakeSimpleSingleUsernameData();
  expected_single_username_data.set_prompt_edit(
      autofill::AutofillUploadContents::EDIT_UNSPECIFIED);
  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

// Verifies that NOT_USERNAME vote is sent on password form if no single
// username typing had preceded single password typing.
TEST_F(VotesUploaderTest, SingleUsernameNoUsernameCandidate) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      features::kUsernameFirstFlowFallbackCrowdsourcing);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(mock_field_manager, GetFieldType).WillByDefault(Return(UNKNOWN_TYPE));
  ON_CALL(client_, GetFieldInfoManager)
      .WillByDefault(Return(&mock_field_manager));

  VotesUploader votes_uploader(&client_, false);
  votes_uploader.set_single_username_vote_data(
      FieldRendererId(), std::u16string(), FormPredictions(),
      /*stored_credentials=*/{}, /*password_form_had_username_field=*/false);
  votes_uploader.set_suggested_username(u"");
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(/*saved_username=*/u"");
#endif  // !BUILDFLAG(IS_ANDROID)

  votes_uploader.MaybeSendSingleUsernameVote();

  // Expect upload on the password form for the fallback classifier.
  autofill::AutofillUploadContents::SingleUsernameData
      expected_single_username_data;
  expected_single_username_data.set_username_form_signature(0);
  expected_single_username_data.set_username_field_signature(0);
  expected_single_username_data.set_value_type(
      autofill::AutofillUploadContents::NO_VALUE_TYPE);

  EXPECT_CALL(
      mock_autofill_download_manager_,
      StartUploadRequest(
          AllOf(SignatureIsSameAs(submitted_form_),
                UploadedSingleUsernameDataIs(expected_single_username_data)),
          _, _, _, _, _, /*observer=*/IsNull()));
  votes_uploader.UploadPasswordVote(submitted_form_, submitted_form_,
                                    autofill::PASSWORD, std::string());
}

TEST_F(VotesUploaderTest, SaveSingleUsernameVote) {
  VotesUploader votes_uploader(&client_, false);

  std::u16string single_username_candidate_value = u"username_candidate_value";
  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, single_username_candidate_value,
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);
#if !BUILDFLAG(IS_ANDROID)
  votes_uploader.CalculateUsernamePromptEditState(
      /*saved_username=*/single_username_candidate_value);
#endif  // !BUILDFLAG(IS_ANDROID)

  // Init store and expect that adding field info is called.
  scoped_refptr<MockPasswordStoreInterface> store =
      new testing::StrictMock<MockPasswordStoreInterface>();

#if BUILDFLAG(IS_ANDROID)
  EXPECT_CALL(*store, GetFieldInfoStore)
      .WillRepeatedly(testing::Return(nullptr));
#else
  MockFieldInfoStore mock_field_store_;
  EXPECT_CALL(*store, GetFieldInfoStore)
      .WillRepeatedly(testing::Return(&mock_field_store_));
  EXPECT_CALL(mock_field_store_,
              AddFieldInfo(FieldInfoHasData(kSingleUsernameFormSignature,
                                            kSingleUsernameFieldSignature,
                                            SINGLE_USERNAME)));
#endif  // BUILDFLAG(IS_ANDROID)

  // Init FieldInfoManager.
  FieldInfoManagerImpl field_info_manager(store);
  EXPECT_CALL(client_, GetFieldInfoManager())
      .WillRepeatedly(Return(&field_info_manager));

  votes_uploader.MaybeSendSingleUsernameVote();
  task_environment_.RunUntilIdle();
}

TEST_F(VotesUploaderTest, DontUploadSingleUsernameWhenAlreadyUploaded) {
  VotesUploader votes_uploader(&client_, false);

  MockFieldInfoManager mock_field_manager;
  ON_CALL(client_, GetFieldInfoManager())
      .WillByDefault(Return(&mock_field_manager));

  // Simulate that the vote has been already uploaded.
  ON_CALL(mock_field_manager, GetFieldType(kSingleUsernameFormSignature,
                                           kSingleUsernameFieldSignature))
      .WillByDefault(Return(SINGLE_USERNAME));

  votes_uploader.set_single_username_vote_data(
      kSingleUsernameRendererId, u"username_candidate_value",
      MakeSimpleSingleUsernamePredictions(), /*stored_credentials=*/{},
      /*password_form_had_username_field=*/false);

  // Expect no upload on the username form, since the vote has been already
  // uploaded.
  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(SignatureIs(kSingleUsernameFormSignature), _,
                                 _, _, _, _, /*observer=*/IsNull()))
      .Times(0);

  votes_uploader.MaybeSendSingleUsernameVote();
}

// Tests FieldNameCollisionInVotes metric reports "true" when multiple fields in
// the form to be uploaded have the same name.
TEST_F(VotesUploaderTest, FieldNameCollisionInVotes) {
  VotesUploader votes_uploader(&client_, false);
  std::u16string password_element = GetFieldNameByIndex(5);
  form_to_upload_.password_element = password_element;
  submitted_form_.password_element = password_element;
  form_to_upload_.confirmation_password_element = password_element;
  submitted_form_.confirmation_password_element = password_element;
  ServerFieldTypeSet expected_field_types = {CONFIRMATION_PASSWORD};

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, false, expected_field_types,
                                 login_form_signature_, true,
                                 /* pref_service= */ nullptr,
                                 /*observer=*/IsNull()));
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FieldNameCollisionInVotes", true, 1);
}

// Tests FieldNameCollisionInVotes metric reports "false" when all fields in the
// form to be uploaded have different names.
TEST_F(VotesUploaderTest, NoFieldNameCollisionInVotes) {
  VotesUploader votes_uploader(&client_, false);
  std::u16string password_element = GetFieldNameByIndex(5);
  std::u16string confirmation_element = GetFieldNameByIndex(12);
  form_to_upload_.password_element = password_element;
  submitted_form_.password_element = password_element;
  form_to_upload_.confirmation_password_element = confirmation_element;
  submitted_form_.confirmation_password_element = confirmation_element;
  ServerFieldTypeSet expected_field_types = {PASSWORD, CONFIRMATION_PASSWORD};

  EXPECT_CALL(mock_autofill_download_manager_,
              StartUploadRequest(_, false, expected_field_types,
                                 login_form_signature_, true,
                                 /* pref_service= */ nullptr,
                                 /*observer=*/IsNull()));
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(votes_uploader.UploadPasswordVote(
      form_to_upload_, submitted_form_, PASSWORD, login_form_signature_));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.FieldNameCollisionInVotes", false, 1);
}

}  // namespace password_manager
