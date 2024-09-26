// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webid/federated_identity_permission_context.h"

#include "chrome/browser/webid/federated_identity_account_keyed_permission_context.h"
#include "chrome/browser/webid/federated_identity_identity_provider_registration_context.h"
#include "chrome/browser/webid/federated_identity_identity_provider_signin_status_context.h"
#include "content/public/browser/browser_context.h"

namespace {
const char kActiveSessionIdpKey[] = "identity-provider";
const char kSharingIdpKey[] = "idp-origin";

}  // namespace

FederatedIdentityPermissionContext::FederatedIdentityPermissionContext(
    content::BrowserContext* browser_context)
    : active_session_context_(
          new FederatedIdentityAccountKeyedPermissionContext(
              browser_context,
              ContentSettingsType::FEDERATED_IDENTITY_ACTIVE_SESSION,
              kActiveSessionIdpKey)),
      sharing_context_(new FederatedIdentityAccountKeyedPermissionContext(
          browser_context,
          ContentSettingsType::FEDERATED_IDENTITY_SHARING,
          kSharingIdpKey)),
      idp_signin_context_(
          new FederatedIdentityIdentityProviderSigninStatusContext(
              browser_context)),
      idp_registration_context_(
          new FederatedIdentityIdentityProviderRegistrationContext(
              browser_context)) {}

FederatedIdentityPermissionContext::~FederatedIdentityPermissionContext() =
    default;

void FederatedIdentityPermissionContext::AddIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.AddObserver(observer);
}

void FederatedIdentityPermissionContext::RemoveIdpSigninStatusObserver(
    IdpSigninStatusObserver* observer) {
  idp_signin_status_observer_list_.RemoveObserver(observer);
}

bool FederatedIdentityPermissionContext::HasActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  return active_session_context_->HasPermission(
      relying_party_requester, relying_party_requester, identity_provider,
      account_identifier);
}

void FederatedIdentityPermissionContext::GrantActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_session_context_->GrantPermission(
      relying_party_requester, relying_party_requester, identity_provider,
      account_identifier);
}

void FederatedIdentityPermissionContext::RevokeActiveSession(
    const url::Origin& relying_party_requester,
    const url::Origin& identity_provider,
    const std::string& account_identifier) {
  active_session_context_->RevokePermission(
      relying_party_requester, relying_party_requester, identity_provider,
      account_identifier);
}

bool FederatedIdentityPermissionContext::HasSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  return sharing_context_->HasPermission(relying_party_requester,
                                         relying_party_embedder,
                                         identity_provider, account_id);
}

void FederatedIdentityPermissionContext::GrantSharingPermission(
    const url::Origin& relying_party_requester,
    const url::Origin& relying_party_embedder,
    const url::Origin& identity_provider,
    const std::string& account_id) {
  sharing_context_->GrantPermission(relying_party_requester,
                                    relying_party_embedder, identity_provider,
                                    account_id);
}

absl::optional<bool> FederatedIdentityPermissionContext::GetIdpSigninStatus(
    const url::Origin& idp_origin) {
  return idp_signin_context_->GetSigninStatus(idp_origin);
}

void FederatedIdentityPermissionContext::SetIdpSigninStatus(
    const url::Origin& idp_origin,
    bool idp_signin_status) {
  absl::optional<bool> old_idp_signin_status = GetIdpSigninStatus(idp_origin);
  if (idp_signin_status == old_idp_signin_status) {
    return;
  }

  idp_signin_context_->SetSigninStatus(idp_origin, idp_signin_status);
  for (IdpSigninStatusObserver& observer : idp_signin_status_observer_list_) {
    observer.OnIdpSigninStatusChanged(idp_origin, idp_signin_status);
  }
}

std::vector<GURL> FederatedIdentityPermissionContext::GetRegisteredIdPs() {
  return idp_registration_context_->GetRegisteredIdPs();
}

void FederatedIdentityPermissionContext::RegisterIdP(const GURL& origin) {
  idp_registration_context_->RegisterIdP(origin);
}

void FederatedIdentityPermissionContext::UnregisterIdP(const GURL& origin) {
  idp_registration_context_->UnregisterIdP(origin);
}

void FederatedIdentityPermissionContext::FlushScheduledSaveSettingsCalls() {
  active_session_context_->FlushScheduledSaveSettingsCalls();
  sharing_context_->FlushScheduledSaveSettingsCalls();
  idp_signin_context_->FlushScheduledSaveSettingsCalls();
  idp_registration_context_->FlushScheduledSaveSettingsCalls();
}
