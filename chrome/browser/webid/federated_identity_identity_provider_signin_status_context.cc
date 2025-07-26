// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"

#include <optional>
#include <vector>

#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/object_permission_context_base.h"
#include "third_party/blink/public/common/webid/login_status_account.h"
#include "third_party/blink/public/common/webid/login_status_options.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::common::webid::LoginStatusAccount;
using blink::common::webid::LoginStatusOptions;

namespace {

// Toplevel key-value store keys.
const char kIdpKey[] = "idp-origin";
const char kIdpSigninStatusKey[] = "idp-signin-status";
const char kIdpSigninOptionsKey[] = "idp-signin-options";

// Keys for LoginStatusOptions object fields
const char kIdpSigninOptionsAccountsKey[] = "accounts";
const char kIdpSigninOptionsExpirationKey[] = "expiration";

// Key for keeping track of when the options were last updated for the
// purpose of expiration.
const char kIdpSigninOptionsLastModifiedKey[] = "last-modified";

// Keys for account properties within LoginStatusOptions accounts list.
const char kId[] = "id";
const char kEmail[] = "email";
const char kName[] = "name";
const char kGivenName[] = "given-name";
const char kPicture[] = "picture";

base::Value::Dict DictFromAccount(const LoginStatusAccount& account) {
  base::Value::Dict dict;
  dict.Set(kId, account.id);
  dict.Set(kEmail, account.email);
  dict.Set(kName, account.name);
  if (account.given_name.has_value()) {
    dict.Set(kGivenName, account.given_name.value());
  }

  if (account.picture.has_value()) {
    dict.Set(kPicture, account.picture.value().spec());
  }
  return dict;
}

std::optional<LoginStatusAccount> AccountFromDict(const base::Value& value) {
  const base::Value::Dict* dict = value.GetIfDict();
  if (!dict) {
    return std::nullopt;
  }
  const std::string* id = dict->FindString(kId);
  const std::string* email = dict->FindString(kEmail);
  const std::string* name = dict->FindString(kName);
  const std::string* given_name = dict->FindString(kGivenName);
  const std::string* picture = dict->FindString(kPicture);

  std::optional<GURL> picture_url =
      picture ? std::make_optional(GURL(*picture)) : std::nullopt;

  // These are required fields in the IDL type IdentityProviderAccount; if
  // they aren't present this isn't usable.
  if (id && email && name) {
    return std::make_optional(LoginStatusAccount(
        *id, *email, *name, base::optional_ref(given_name), picture_url));
  } else {
    return std::nullopt;
  }
}

base::Value::Dict DictFromLoginStatusOptions(
    const LoginStatusOptions& options) {
  base::Value::Dict dict;

  dict.Set(kIdpSigninOptionsLastModifiedKey,
           base::TimeToValue(base::Time::Now()));

  if (options.expiration.has_value()) {
    dict.Set(kIdpSigninOptionsExpirationKey,
             base::TimeDeltaToValue(options.expiration.value()));
  }

  base::Value::List accounts;
  for (const auto& input_account : options.accounts) {
    accounts.Append(DictFromAccount(input_account));
  }
  dict.Set(kIdpSigninOptionsAccountsKey, std::move(accounts));

  return dict;
}

LoginStatusOptions LoginStatusOptionsFromDict(base::Value::Dict* dict) {
  CHECK(dict);
  LoginStatusOptions options;

  base::Value::List* accounts_list =
      dict->FindList(kIdpSigninOptionsAccountsKey);

  if (!accounts_list) {
    return options;
  }

  options.expiration =
      base::ValueToTimeDelta(dict->Find(kIdpSigninOptionsExpirationKey));
  for (const auto& account_value : *accounts_list) {
    std::optional<LoginStatusAccount> maybe_account =
        AccountFromDict(account_value);
    if (maybe_account.has_value()) {
      options.accounts.emplace_back(std::move(maybe_account.value()));
    }
  }
  return options;
}
// Take a dictionary representing the stored accounts list and expiration
// information for a given IdP and returns "true" if the current time exceeds
// the last modified time plus the expiration duration.
bool IsExpired(const base::Value::Dict* options) {
  CHECK(options);
  std::optional<base::TimeDelta> expiration =
      base::ValueToTimeDelta(options->Find(kIdpSigninOptionsExpirationKey));
  std::optional<base::Time> last_modified =
      base::ValueToTime(options->Find(kIdpSigninOptionsLastModifiedKey));
  if (!last_modified) {
    // A valid last_modified time should always be available; if it's not
    // available it implies that the configuration has been corrupted and should
    // be treated as though it is expired.
    return true;
  }
  if (expiration) {
    return (last_modified.value() + expiration.value()) < base::Time::Now();
  } else {
    return false;
  }
}

}  // namespace

FederatedIdentityIdentityProviderSigninStatusContext::
    FederatedIdentityIdentityProviderSigninStatusContext(
        content::BrowserContext* browser_context)
    : ObjectPermissionContextBase(
          ContentSettingsType::
              FEDERATED_IDENTITY_IDENTITY_PROVIDER_SIGNIN_STATUS,
          HostContentSettingsMapFactory::GetForProfile(
              Profile::FromBrowserContext(browser_context))) {}

std::optional<bool>
FederatedIdentityIdentityProviderSigninStatusContext::GetSigninStatus(
    const url::Origin& identity_provider) {
  auto granted_object =
      GetGrantedObject(identity_provider, identity_provider.Serialize());
  if (!granted_object)
    return std::nullopt;

  return granted_object->value.FindBool(kIdpSigninStatusKey);
}

std::vector<LoginStatusAccount>
FederatedIdentityIdentityProviderSigninStatusContext::GetAccountProfiles(
    const url::Origin& identity_provider) {
  auto granted_object =
      GetGrantedObject(identity_provider, identity_provider.Serialize());

  if (granted_object) {
    bool is_logged_in =
        granted_object->value.FindBool(kIdpSigninStatusKey).value_or(false);
    base::Value::Dict* options_dict =
        granted_object->value.FindDict(kIdpSigninOptionsKey);
    if (is_logged_in && options_dict && !IsExpired(options_dict)) {
      return LoginStatusOptionsFromDict(options_dict).accounts;
    }
  }

  return {};
}

void FederatedIdentityIdentityProviderSigninStatusContext::SetSigninStatus(
    const url::Origin& identity_provider,
    bool signin_status,
    base::optional_ref<const blink::common::webid::LoginStatusOptions>
        options) {
  if (identity_provider.opaque()) {
    mojo::ReportBadMessage("Bad IdP Origin from renderer.");
    return;
  }

  base::Value::Dict new_object;
  new_object.Set(kIdpKey, identity_provider.Serialize());
  new_object.Set(kIdpSigninStatusKey, base::Value(signin_status));

  if (signin_status && options.has_value()) {
    new_object.Set(kIdpSigninOptionsKey,
                   DictFromLoginStatusOptions(options.value()));
  }

  auto granted_object =
      GetGrantedObject(identity_provider, identity_provider.Serialize());

  if (granted_object) {
    // Currently, if an existing grant and set of IdP-supplied options exists,
    // calling setStatus with no options or a Login-Status: Logged-In response
    // header will not refresh the timestamp for the purpose of expiration,
    // but still needs to preserve any existing IDP options. If the options
    // have already expired, we can safely discard them.
    if (!options.has_value()) {
      base::Value::Dict* existing_options =
          granted_object->value.FindDict(kIdpSigninOptionsKey);
      if (signin_status && existing_options && !IsExpired(existing_options)) {
        new_object.Set(kIdpSigninOptionsKey, existing_options->Clone());
      }
    }

    UpdateObjectPermission(identity_provider, granted_object->value,
                           std::move(new_object));
  } else {
    GrantObjectPermission(identity_provider, std::move(new_object));
  }
}

std::string
FederatedIdentityIdentityProviderSigninStatusContext::GetKeyForObject(
    const base::Value::Dict& object) {
  return *object.FindString(kIdpKey);
}

bool FederatedIdentityIdentityProviderSigninStatusContext::IsValidObject(
    const base::Value::Dict& object) {
  return object.FindString(kIdpKey);
}

void FederatedIdentityIdentityProviderSigninStatusContext::
    FlushScheduledSaveSettingsCalls() {
  // Replace any valid-but-expired entries with a copy that only contains the
  // basic login status
  for (const auto& granted_object : GetAllGrantedObjects()) {
    base::Value::Dict* options_dict =
        granted_object->value.FindDict(kIdpSigninOptionsKey);
    if (IsValidObject(granted_object->value) && options_dict &&
        IsExpired(options_dict)) {
      base::Value::Dict new_object;
      url::Origin identity_provider =
          url::Origin::Create(GURL(*granted_object->value.FindString(kIdpKey)));
      bool signin_status =
          granted_object->value.FindBool(kIdpSigninStatusKey).value_or(false);
      new_object.Set(kIdpKey, identity_provider.Serialize());
      new_object.Set(kIdpSigninStatusKey, base::Value(signin_status));
      UpdateObjectPermission(identity_provider, granted_object->value,
                             std::move(new_object));
    }
  }
  permissions::ObjectPermissionContextBase::FlushScheduledSaveSettingsCalls();
}

std::u16string
FederatedIdentityIdentityProviderSigninStatusContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  DCHECK(IsValidObject(object));
  return base::UTF8ToUTF16(*object.FindString(kIdpKey));
}
