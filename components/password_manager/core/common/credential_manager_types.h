// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_

#include <stddef.h>

#include <memory>
#include <ostream>
#include <string>

#include "base/compiler_specific.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

// Limit the size of the federations array that we pass to the browser to
// something reasonably sane.
const size_t kMaxFederations = 50u;

enum class CredentialType {
  CREDENTIAL_TYPE_EMPTY = 0,
  CREDENTIAL_TYPE_PASSWORD,
  CREDENTIAL_TYPE_FEDERATED,
  CREDENTIAL_TYPE_LAST = CREDENTIAL_TYPE_FEDERATED
};

enum class CredentialManagerError {
  SUCCESS,
  PENDING_REQUEST,
  PASSWORDSTOREUNAVAILABLE,
  UNKNOWN,
};

enum class CredentialMediationRequirement { kSilent, kOptional, kRequired };

std::string CredentialTypeToString(CredentialType value);
std::ostream& operator<<(std::ostream& os, CredentialType value);

struct CredentialInfo {
  CredentialInfo();
  CredentialInfo(CredentialType type,
                 absl::optional<std::u16string> id,
                 absl::optional<std::u16string> name,
                 GURL icon,
                 absl::optional<std::u16string> password,
                 url::Origin federation);

  CredentialInfo(const CredentialInfo& other);
  ~CredentialInfo();

  bool operator==(const CredentialInfo& rhs) const;

  CredentialType type = CredentialType::CREDENTIAL_TYPE_EMPTY;

  // An identifier (username, email address, etc). Corresponds to
  // WebCredential's id property.
  absl::optional<std::u16string> id;

  // An user-friendly name ("Jane Doe"). Corresponds to WebCredential's name
  // property.
  absl::optional<std::u16string> name;

  // The address of this credential's icon (e.g. the user's avatar).
  // Corresponds to WebCredential's icon property.
  GURL icon;

  // Corresponds to WebPasswordCredential's password property.
  absl::optional<std::u16string> password;

  // Corresponds to WebFederatedCredential's provider property.
  url::Origin federation;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_COMMON_CREDENTIAL_MANAGER_TYPES_H_
