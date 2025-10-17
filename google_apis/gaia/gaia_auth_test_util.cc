// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_auth_test_util.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/list_accounts_response.pb.h"
#include "google_apis/gaia/oauth2_mint_token_consent_result.pb.h"

namespace gaia {

std::string GenerateOAuth2MintTokenConsentResult(
    std::optional<bool> approved,
    const std::optional<std::string>& encrypted_approval_data,
    const std::optional<GaiaId>& obfuscated_id,
    base::Base64UrlEncodePolicy encode_policy) {
  OAuth2MintTokenConsentResult consent_result;
  if (approved.has_value()) {
    consent_result.set_approved(approved.value());
  }
  if (encrypted_approval_data.has_value()) {
    consent_result.set_encrypted_approval_data(encrypted_approval_data.value());
  }
  if (obfuscated_id.has_value()) {
    consent_result.set_obfuscated_id(obfuscated_id->ToString());
  }
  std::string serialized_consent = consent_result.SerializeAsString();
  std::string encoded_consent;
  base::Base64UrlEncode(serialized_consent, encode_policy, &encoded_consent);
  return encoded_consent;
}

std::string CreateListAccountsResponseInBinaryFormat(
    const std::vector<gaia::CookieParams>& params) {
  gaia::ListAccountsResponse response;

  for (const auto& param : params) {
    gaia::Account* account = response.add_account();

    account->set_display_email(param.email);
    account->set_valid_session(param.valid);
    account->set_obfuscated_id(param.gaia_id.ToString());
    account->set_signed_out(param.signed_out);
    account->set_is_verified(param.verified);
  }

  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  return base::Base64Encode(serialized_response);
}

std::string CreateListAccountsResponseInLegacyFormat(
    const std::vector<gaia::CookieParams>& params) {
  std::vector<std::string> response_body;

  for (const auto& param : params) {
    std::string response_part = base::StringPrintf(
        "[\"b\", 0, \"n\", \"%s\", \"p\", 0, 0, 0, 0, %d, \"%s\"",
        param.email.c_str(), param.valid ? 1 : 0,
        param.gaia_id.ToString().c_str());

    if (param.signed_out || !param.verified) {
      response_part +=
          base::StringPrintf(", null, null, null, %d, %d",
                             param.signed_out ? 1 : 0, param.verified ? 1 : 0);
    }

    response_part += "]";
    response_body.push_back(response_part);
  }

  return std::string("[\"f\", [") + base::JoinString(response_body, ", ") +
         "]]";
}

}  // namespace gaia
