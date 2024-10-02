// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/stub_password_manager_driver.h"

#include "url/gurl.h"

namespace password_manager {

StubPasswordManagerDriver::StubPasswordManagerDriver() = default;
StubPasswordManagerDriver::~StubPasswordManagerDriver() = default;

int StubPasswordManagerDriver::GetId() const {
  return 0;
}

void StubPasswordManagerDriver::SetPasswordFillData(
    const autofill::PasswordFormFillData& form_data) {}

void StubPasswordManagerDriver::GeneratedPasswordAccepted(
    const std::u16string& password) {}

void StubPasswordManagerDriver::FillSuggestion(const std::u16string& username,
                                               const std::u16string& password) {
}

#if BUILDFLAG(IS_ANDROID)
void StubPasswordManagerDriver::TriggerFormSubmission() {}
#endif

void StubPasswordManagerDriver::PreviewSuggestion(
    const std::u16string& username,
    const std::u16string& password) {}

void StubPasswordManagerDriver::PreviewGenerationSuggestion(
    const std::u16string& password) {}

void StubPasswordManagerDriver::ClearPreviewedForm() {
}

void StubPasswordManagerDriver::SetSuggestionAvailability(
    autofill::FieldRendererId generation_element_id,
    const autofill::mojom::AutofillState state) {}

PasswordGenerationFrameHelper*
StubPasswordManagerDriver::GetPasswordGenerationHelper() {
  return nullptr;
}

PasswordManagerInterface* StubPasswordManagerDriver::GetPasswordManager() {
  return nullptr;
}

PasswordAutofillManager*
StubPasswordManagerDriver::GetPasswordAutofillManager() {
  return nullptr;
}

bool StubPasswordManagerDriver::IsInPrimaryMainFrame() const {
  return true;
}

bool StubPasswordManagerDriver::CanShowAutofillUi() const {
  return true;
}

::ui::AXTreeID StubPasswordManagerDriver::GetAxTreeId() const {
  return {};
}

int StubPasswordManagerDriver::GetFrameId() const {
  return GetId();
}

const GURL& StubPasswordManagerDriver::GetLastCommittedURL() const {
  return GURL::EmptyGURL();
}

}  // namespace password_manager
