// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/crowdsourcing/test_votes_uploader.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/crowdsourcing/votes_uploader_test_api.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestVotesUploader::TestVotesUploader(AutofillClient* client)
    : VotesUploader(client) {}
TestVotesUploader::~TestVotesUploader() = default;

void TestVotesUploader::UploadVote(
    std::unique_ptr<FormStructure> submitted_form,
    base::TimeTicks initial_interaction_timestamp,
    base::TimeTicks submission_timestamp,
    bool observed_submission,
    const std::u16string& last_unlocked_credit_card_cvc,
    const ukm::SourceId source_id) {
  submitted_form_signature_ = submitted_form->FormSignatureAsStr();

  if (expected_observed_submission_ != std::nullopt) {
    EXPECT_EQ(expected_observed_submission_, observed_submission);
  }

  // If we have expected field types set, make sure they match.
  if (!expected_submitted_field_types_.empty()) {
    ASSERT_EQ(expected_submitted_field_types_.size(),
              submitted_form->field_count());
    for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
      SCOPED_TRACE(base::StringPrintf(
          "Field %d with value %s", static_cast<int>(i),
          base::UTF16ToUTF8(
              submitted_form->field(i)->value(ValueSemantics::kCurrent))
              .c_str()));
      const FieldTypeSet& possible_types =
          submitted_form->field(i)->possible_types();
      EXPECT_EQ(expected_submitted_field_types_[i].size(),
                possible_types.size());
      for (auto it : expected_submitted_field_types_[i]) {
        EXPECT_TRUE(possible_types.count(it))
            << "Expected type: " << FieldTypeToStringView(it);
      }
    }
  }

  VotesUploader::UploadVote(std::move(submitted_form),
                            initial_interaction_timestamp, submission_timestamp,
                            observed_submission, last_unlocked_credit_card_cvc,
                            source_id);
}

bool TestVotesUploader::MaybeStartVoteUploadProcess(
    std::unique_ptr<FormStructure> form,
    bool observed_submission,
    LanguageCode current_page_language,
    base::TimeTicks initial_interaction_timestamp,
    const std::u16string& last_unlocked_credit_card_cvc,
    ukm::SourceId ukm_source_id) {
  FormStructure* form_ptr = form.get();
  if (VotesUploader::MaybeStartVoteUploadProcess(
          std::move(form), observed_submission, current_page_language,
          initial_interaction_timestamp, last_unlocked_credit_card_cvc,
          ukm_source_id)) {
    last_uploaded_form_associations_ = form_ptr->form_associations();
    // The purpose of this runloop is to ensure that the field type
    // determination finishes.
    base::RunLoop run_loop;
    // Since the `vote_upload_task_runner()` is a `base::SequencedTaskRunner`,
    // the `run_loop` is quit only after the task and reply posted by
    // MaybeStartVoteUploadProcess() is finished.
    test_api(*this).task_runner().PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                                   run_loop.QuitClosure());
    run_loop.Run();
    return true;
  }
  return false;
}

}  // namespace autofill
