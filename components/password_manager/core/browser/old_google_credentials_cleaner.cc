// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"

#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

OldGoogleCredentialCleaner::OldGoogleCredentialCleaner(
    scoped_refptr<PasswordStoreInterface> store,
    PrefService* prefs)
    : store_(std::move(store)), prefs_(prefs) {}

OldGoogleCredentialCleaner::~OldGoogleCredentialCleaner() = default;

bool OldGoogleCredentialCleaner::NeedsCleaning() {
  return !prefs_->GetBoolean(prefs::kWereOldGoogleLoginsRemoved);
}

void OldGoogleCredentialCleaner::StartCleaning(Observer* observer) {
  DCHECK(observer);
  DCHECK(!observer_);
  observer_ = observer;
  store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void OldGoogleCredentialCleaner::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<PasswordForm>> results) {
  base::Time cutoff;  // the null time
  static const base::Time::Exploded exploded_cutoff = {
      2012, 1, 0, 1, 0, 0, 0, 0};  // 00:00 Jan 1 2012
  bool conversion_success =
      base::Time::FromUTCExploded(exploded_cutoff, &cutoff);
  DCHECK(conversion_success);

  auto IsOldGoogleForm = [&cutoff](const std::unique_ptr<PasswordForm>& form) {
    return (form->scheme == PasswordForm::Scheme::kHtml &&
            (form->signon_realm == "http://www.google.com" ||
             form->signon_realm == "http://www.google.com/" ||
             form->signon_realm == "https://www.google.com" ||
             form->signon_realm == "https://www.google.com/")) &&
           form->date_created < cutoff;
  };

  for (const auto& form : results) {
    if (IsOldGoogleForm(form)) {
      store_->RemoveLogin(*form);
    }
  }
  prefs_->SetBoolean(prefs::kWereOldGoogleLoginsRemoved, true);
  observer_->CleaningCompleted();
}

}  // namespace password_manager
