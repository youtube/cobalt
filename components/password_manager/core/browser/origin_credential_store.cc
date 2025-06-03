// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/origin_credential_store.h"

#include <ios>
#include <tuple>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/url_formatter/elide_url.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

using BlocklistedStatus = OriginCredentialStore::BlocklistedStatus;

UiCredential::UiCredential(std::u16string username,
                           std::u16string password,
                           url::Origin origin,
                           password_manager_util::GetLoginMatchType match_type,
                           base::Time last_used)
    : username_(std::move(username)),
      password_(std::move(password)),
      origin_(std::move(origin)),
      match_type_(match_type),
      last_used_(last_used) {}

UiCredential::UiCredential(const PasswordForm& form,
                           const url::Origin& affiliated_origin)
    : username_(form.username_value),
      password_(form.password_value),
      match_type_(password_manager_util::GetMatchType(form)),
      last_used_(form.date_last_used) {
  FacetURI facet_uri = FacetURI::FromPotentiallyInvalidSpec(form.signon_realm);
  if (facet_uri.IsValidAndroidFacetURI()) {
    origin_ = affiliated_origin;
    display_name_ = form.app_display_name.empty()
                        ? facet_uri.GetAndroidPackageDisplayName()
                        : form.app_display_name;
  } else {
    origin_ = url::Origin::Create(form.url);
    display_name_ =
        base::UTF16ToUTF8(url_formatter::FormatOriginForSecurityDisplay(
            url::Origin::Create(form.url),
            url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  }
}

UiCredential::UiCredential(UiCredential&&) = default;
UiCredential::UiCredential(const UiCredential&) = default;
UiCredential& UiCredential::operator=(UiCredential&&) = default;
UiCredential& UiCredential::operator=(const UiCredential&) = default;
UiCredential::~UiCredential() = default;

bool operator==(const UiCredential& lhs, const UiCredential& rhs) {
  auto tie = [](const UiCredential& cred) {
    return std::make_tuple(std::cref(cred.username()),
                           std::cref(cred.password()), std::cref(cred.origin()),
                           cred.match_type(), cred.last_used());
  };

  return tie(lhs) == tie(rhs);
}

std::ostream& operator<<(std::ostream& os, const UiCredential& credential) {
  std::string match_type;
  switch (credential.match_type()) {
    case password_manager_util::GetLoginMatchType::kExact:
      match_type = "exact match";
      break;
    case password_manager_util::GetLoginMatchType::kAffiliated:
      match_type = "affiliated match";
      break;
    case password_manager_util::GetLoginMatchType::kPSL:
      match_type = "PSL match";
      break;
  }
  return os << "(user: \"" << credential.username() << "\", "
            << "pwd: \"" << credential.password() << "\", "
            << "origin: \"" << credential.origin() << "\", " << match_type
            << ", "
            << "last_used: " << credential.last_used();
}

OriginCredentialStore::OriginCredentialStore(url::Origin origin)
    : origin_(std::move(origin)) {}
OriginCredentialStore::~OriginCredentialStore() = default;

void OriginCredentialStore::SaveCredentials(
    std::vector<UiCredential> credentials) {
  credentials_ = std::move(credentials);
}

base::span<const UiCredential> OriginCredentialStore::GetCredentials() const {
  return credentials_;
}

void OriginCredentialStore::SetBlocklistedStatus(bool is_blocklisted) {
  if (is_blocklisted) {
    blocklisted_status_ = BlocklistedStatus::kIsBlocklisted;
    return;
  }

  if (blocklisted_status_ == BlocklistedStatus::kIsBlocklisted) {
    blocklisted_status_ = BlocklistedStatus::kWasBlocklisted;
  }
}

BlocklistedStatus OriginCredentialStore::GetBlocklistedStatus() const {
  return blocklisted_status_;
}

void OriginCredentialStore::ClearCredentials() {
  credentials_.clear();
}

}  // namespace password_manager
