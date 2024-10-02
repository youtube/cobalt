// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_saver_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill::FormData;
using autofill::FormFieldData;

namespace password_manager {

namespace {

// Remove all information from |form| that is not required for signature
// calculation.
void SanitizeFormData(FormData* form) {
  form->main_frame_origin = url::Origin();
  for (FormFieldData& field : form->fields) {
    field.label.clear();
    field.value.clear();
    field.autocomplete_attribute.clear();
    field.options.clear();
    field.placeholder.clear();
    field.css_classes.clear();
    field.id_attribute.clear();
    field.name_attribute.clear();
  }
}

// Do the clean up of |matches| after |pending| was just pushed to the store.
void PostProcessMatches(const PasswordForm& pending,
                        const std::vector<const PasswordForm*>& matches,
                        const std::u16string& old_password,
                        PasswordStoreInterface* store) {
  DCHECK(!pending.blocked_by_user);

  // Update existing matches in the password store.
  for (const auto* match : matches) {
    if (match->IsFederatedCredential() ||
        ArePasswordFormUniqueKeysEqual(pending, *match))
      continue;
    // Delete obsolete empty username credentials.
    const bool same_password = match->password_value == pending.password_value;
    const bool username_was_added =
        match->username_value.empty() && !pending.username_value.empty();
    if (same_password && username_was_added &&
        password_manager_util::GetMatchType(*match) ==
            password_manager_util::GetLoginMatchType::kExact) {
      store->RemoveLogin(*match);
      continue;
    }
    const bool same_username = match->username_value == pending.username_value;
    if (same_username) {
      // Maybe update the password value.
      const bool form_has_old_password = match->password_value == old_password;
      if (form_has_old_password) {
        PasswordForm form_to_update = *match;
        form_to_update.password_value = pending.password_value;
        form_to_update.date_password_modified = base::Time::Now();
        SanitizeFormData(&form_to_update.form_data);
        store->UpdateLogin(std::move(form_to_update));
      }
    }
  }
}

}  // namespace

FormSaverImpl::FormSaverImpl(PasswordStoreInterface* store) : store_(store) {
  DCHECK(store);
}

FormSaverImpl::~FormSaverImpl() = default;

PasswordForm FormSaverImpl::Blocklist(PasswordFormDigest digest) {
  PasswordForm blocklisted =
      password_manager_util::MakeNormalizedBlocklistedForm(std::move(digest));
  blocklisted.date_created = base::Time::Now();
  store_->AddLogin(blocklisted);
  return blocklisted;
}

void FormSaverImpl::Unblocklist(const PasswordFormDigest& digest) {
  store_->Unblocklist(digest);
}

void FormSaverImpl::Save(PasswordForm pending,
                         const std::vector<const PasswordForm*>& matches,
                         const std::u16string& old_password) {
  SanitizeFormData(&pending.form_data);
  pending.date_password_modified = base::Time::Now();
  store_->AddLogin(pending);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::Update(PasswordForm pending,
                           const std::vector<const PasswordForm*>& matches,
                           const std::u16string& old_password) {
  SanitizeFormData(&pending.form_data);
  if (old_password != pending.password_value)
    pending.date_password_modified = base::Time::Now();
  store_->UpdateLogin(pending);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::UpdateReplace(
    PasswordForm pending,
    const std::vector<const PasswordForm*>& matches,
    const std::u16string& old_password,
    const PasswordForm& old_unique_key) {
  SanitizeFormData(&pending.form_data);
  pending.date_password_modified = base::Time::Now();
  store_->UpdateLoginWithPrimaryKey(pending, old_unique_key);
  // Update existing matches in the password store.
  PostProcessMatches(pending, matches, old_password, store_);
}

void FormSaverImpl::Remove(const PasswordForm& form) {
  store_->RemoveLogin(form);
}

std::unique_ptr<FormSaver> FormSaverImpl::Clone() {
  return std::make_unique<FormSaverImpl>(store_);
}

}  // namespace password_manager
