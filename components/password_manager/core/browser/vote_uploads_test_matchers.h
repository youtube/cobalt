// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_

#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_utils/vote_uploads_test_matchers.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/votes_uploader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

using ::testing::AllOf;
using ::testing::ContainerEq;
using ::testing::Field;
using ::testing::IsFalse;
using ::testing::Optional;
using ::testing::Property;
using ::testing::ResultOf;

inline auto SignatureIsSameAs(const PasswordForm& form) {
  return autofill::SignatureIsSameAs(form.form_data);
}

MATCHER_P(HasGenerationVote, expect_generation_vote, "") {
  bool found_generation_vote = false;
  for (const auto& field : arg) {
    if (field->generation_type() !=
        autofill::AutofillUploadContents::Field::NO_GENERATION) {
      found_generation_vote = true;
      break;
    }
  }
  return found_generation_vote == expect_generation_vote;
}

inline auto VoteTypesAre(VoteTypeMap expected) {
  static constexpr auto kNoInformation =
      autofill::AutofillUploadContents::Field::NO_INFORMATION;
  auto get_vote_types = [](const autofill::FormStructure& actual) {
    VoteTypeMap vote_types;
    for (const auto& field : actual) {
      if (field->vote_type() != kNoInformation) {
        vote_types[field->name] = field->vote_type();
      }
    }
    return vote_types;
  };
  base::EraseIf(expected,
                [](const auto& p) { return p.second == kNoInformation; });
  return ResultOf("get_vote_types", get_vote_types, ContainerEq(expected));
}

MATCHER_P(UploadedSingleUsernameVoteTypeIs, expected_type, "") {
  for (const auto& field : arg) {
    autofill::ServerFieldType vote = field->possible_types().empty()
                                         ? autofill::UNKNOWN_TYPE
                                         : *field->possible_types().begin();
    if ((vote == autofill::SINGLE_USERNAME || vote == autofill::NOT_USERNAME) &&
        expected_type != field->single_username_vote_type()) {
      // Wrong vote type.
      *result_listener << "Expected vote type for the field " << field->name
                       << " is " << expected_type << ", but found "
                       << field->single_username_vote_type().value();
      return false;
    }
  }
  return true;
}

inline auto UploadedSingleUsernameDataIs(
    const autofill::AutofillUploadContents::SingleUsernameData& expected) {
  using SingleUsernameData =
      autofill::AutofillUploadContents::SingleUsernameData;
  return Property(
      "single_username_data", &autofill::FormStructure::single_username_data,
      Optional(AllOf(Property("username_form_signature",
                              &SingleUsernameData::username_form_signature,
                              expected.username_form_signature()),
                     Property("username_field_signature",
                              &SingleUsernameData::username_field_signature,
                              expected.username_field_signature()),
                     Property("value_type", &SingleUsernameData::value_type,
                              expected.value_type()),
                     Property("prompt_edit", &SingleUsernameData::prompt_edit,
                              expected.prompt_edit()))));
}

inline auto SingleUsernameDataNotUploaded() {
  return Property("single_username_data",
                  &autofill::FormStructure::single_username_data, IsFalse());
}

inline auto PasswordsWereRevealed(bool passwords_were_revealed) {
  return Property("passwords_were_revealed",
                  &autofill::FormStructure::passwords_were_revealed,
                  passwords_were_revealed);
}

MATCHER_P(HasPasswordAttributesVote, is_vote_expected, "") {
  absl::optional<std::pair<autofill::PasswordAttribute, bool>> vote =
      arg.get_password_attributes_vote();
  EXPECT_EQ(is_vote_expected, vote.has_value());
  return true;
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_VOTE_UPLOADS_TEST_MATCHERS_H_
