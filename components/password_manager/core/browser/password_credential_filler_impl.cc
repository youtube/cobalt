// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_credential_filler_impl.h"

#include <string>
#include "base/check.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/common/password_manager_features.h"

namespace {

using autofill::mojom::SubmissionReadinessState;

// Infers whether a form should be submitted based on the feature's state and
// the form's structure (submission_readiness).
bool CalculateTriggerSubmission(SubmissionReadinessState submission_readiness) {
  switch (submission_readiness) {
    case SubmissionReadinessState::kNoInformation:
    case SubmissionReadinessState::kError:
    case SubmissionReadinessState::kNoUsernameField:
    case SubmissionReadinessState::kNoPasswordField:
    case SubmissionReadinessState::kFieldBetweenUsernameAndPassword:
    case SubmissionReadinessState::kFieldAfterPasswordField:
      return false;

    case SubmissionReadinessState::kEmptyFields:
    case SubmissionReadinessState::kMoreThanTwoFields:
    case SubmissionReadinessState::kTwoFields:
      return true;
  }
}

// Returns a prediction whether the form that contains |username_element| and
// |password_element| will be ready for submission after filling these two
// elements.
// TODO(crbug/1462532): This is a replication of the logic in
// password_autofill_agent.cc. Remove the logic in the agent when
// PasswordSuggestionBottomSheetV2 is launched.
SubmissionReadinessState CalculateSubmissionReadiness(
    password_manager::SubmissionReadinessParams params) {
  if (!base::FeatureList::IsEnabled(
          password_manager::features::kPasswordSuggestionBottomSheetV2)) {
    return params.submission_readiness;
  }
  const autofill::FormData& form_data = params.form;
  uint64_t username_index = params.username_field_index;
  uint64_t password_index = params.password_field_index;
  size_t number_of_elements = form_data.fields.size();
  CHECK(username_index <= number_of_elements &&
        password_index <= number_of_elements);
  if (form_data.fields.empty() || ((username_index == number_of_elements) &&
                                   (password_index == number_of_elements))) {
    // This is unexpected. |form| is supposed to contain username or
    // password elements.
    return SubmissionReadinessState::kError;
  }
  if ((username_index == number_of_elements) &&
      (password_index != number_of_elements)) {
    return SubmissionReadinessState::kNoUsernameField;
  }
  if (password_index == number_of_elements) {
    return SubmissionReadinessState::kNoPasswordField;
  }

  auto ShouldIgnoreField = [](const autofill::FormFieldData& field) {
    if (!field.IsFocusable()) {
      return true;
    }
    // Don't treat a checkbox (e.g. "remember me") as an input field that may
    // block a form submission. Note: Don't use |check_status !=
    // kNotCheckable|, a radio button is considered a "checkable" element too,
    // but it should block a submission.
    return field.form_control_type == autofill::FormControlType::kInputCheckbox;
  };

  for (size_t i = username_index + 1; i < password_index; ++i) {
    if (!ShouldIgnoreField(form_data.fields[i])) {
      return SubmissionReadinessState::kFieldBetweenUsernameAndPassword;
    }
  }

  for (size_t i = password_index + 1; i < number_of_elements; ++i) {
    if (!ShouldIgnoreField(form_data.fields[i])) {
      return SubmissionReadinessState::kFieldAfterPasswordField;
    }
  }

  size_t number_of_visible_elements = 0;
  for (size_t i = 0; i < number_of_elements; ++i) {
    if (ShouldIgnoreField(form_data.fields[i])) {
      continue;
    }

    if (username_index != i && password_index != i &&
        form_data.fields[i].value.empty()) {
      return SubmissionReadinessState::kEmptyFields;
    }
    number_of_visible_elements++;
  }

  if (number_of_visible_elements > 2) {
    return SubmissionReadinessState::kMoreThanTwoFields;
  }

  return SubmissionReadinessState::kTwoFields;
}

}  // namespace

namespace password_manager {

PasswordCredentialFillerImpl::PasswordCredentialFillerImpl(
    base::WeakPtr<PasswordManagerDriver> driver,
    const SubmissionReadinessParams& submission_readiness_params)
    : driver_(driver),
      submission_readiness_(
          CalculateSubmissionReadiness(submission_readiness_params)),
      trigger_submission_(CalculateTriggerSubmission(submission_readiness_)) {}

PasswordCredentialFillerImpl::~PasswordCredentialFillerImpl() = default;

void PasswordCredentialFillerImpl::FillUsernameAndPassword(
    const std::u16string& username,
    const std::u16string& password) {
  CHECK(driver_);
  if (!base::FeatureList::IsEnabled(
          features::kPasswordSuggestionBottomSheetV2)) {
    driver_->KeyboardReplacingSurfaceClosed(ToShowVirtualKeyboard(false));
  }

  driver_->FillSuggestion(username, password);

  trigger_submission_ &= !username.empty();

  if (trigger_submission_) {
    // TODO(crbug.com/1283004): As auto-submission has been launched, measuring
    // the time between filling by TTF and submisionn is not crucial. Remove
    // this call, the method itself and the metrics if we are not going to use
    // all that for new launches, e.g. crbug.com/1393043.
    driver_->TriggerFormSubmission();
  }
}

void PasswordCredentialFillerImpl::UpdateTriggerSubmission(bool new_value) {
  trigger_submission_ = new_value;
}

bool PasswordCredentialFillerImpl::ShouldTriggerSubmission() const {
  return trigger_submission_;
}

SubmissionReadinessState
PasswordCredentialFillerImpl::GetSubmissionReadinessState() const {
  return submission_readiness_;
}

const GURL& PasswordCredentialFillerImpl::GetFrameUrl() const {
  CHECK(driver_);
  return driver_->GetLastCommittedURL();
}

void PasswordCredentialFillerImpl::Dismiss(ToShowVirtualKeyboard should_show) {
  // TODO(crbug/1462532): Remove this function once the feature is enabled.
  if (base::FeatureList::IsEnabled(
          features::kPasswordSuggestionBottomSheetV2) ||
      !driver_) {
    return;
  }
  // TODO(crbug/1434278): Avoid using KeyboardReplacingSurfaceClosed.
  driver_->KeyboardReplacingSurfaceClosed(should_show);
}

}  // namespace password_manager
