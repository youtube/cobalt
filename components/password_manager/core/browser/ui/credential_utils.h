// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_

#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "base/template_util.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"

namespace password_manager {

// Simple struct that stores a canonicalized credential. Allows implicit
// constructon from PasswordForm, CredentialUIEntry and LeakCheckCredentail for
// convenience.
struct CanonicalizedCredential {
  CanonicalizedCredential(const PasswordForm& form)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(form.username_value)),
        password(form.password_value) {}

  CanonicalizedCredential(const CredentialUIEntry& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username)),
        password(credential.password) {}

  CanonicalizedCredential(const LeakCheckCredential& credential)  // NOLINT
      : canonicalized_username(CanonicalizeUsername(credential.username())),
        password(credential.password()) {}

  std::u16string canonicalized_username;
  std::u16string password;
};

inline bool operator<(const CanonicalizedCredential& lhs,
                      const CanonicalizedCredential& rhs) {
  return std::tie(lhs.canonicalized_username, lhs.password) <
         std::tie(rhs.canonicalized_username, rhs.password);
}

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_UI_CREDENTIAL_UTILS_H_
