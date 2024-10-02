// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_

#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/federated_identity_auto_reauthn_permission_context_delegate.h"

namespace content {
class BrowserContext;
}

namespace permissions {
class PermissionDecisionAutoBlocker;
}

// Context for storing user permission to use the browser FedCM API's auto
// sign-in feature.
class FederatedIdentityAutoReauthnPermissionContext
    : public content::FederatedIdentityAutoReauthnPermissionContextDelegate,
      public KeyedService {
 public:
  explicit FederatedIdentityAutoReauthnPermissionContext(
      content::BrowserContext* browser_context);

  ~FederatedIdentityAutoReauthnPermissionContext() override;

  FederatedIdentityAutoReauthnPermissionContext(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;
  FederatedIdentityAutoReauthnPermissionContext& operator=(
      const FederatedIdentityAutoReauthnPermissionContext&) = delete;

  // content::FederatedIdentityAutoReauthnPermissionContextDelegate:
  bool HasAutoReauthnContentSetting() override;
  bool IsAutoReauthnEmbargoed(
      const url::Origin& relying_party_embedder) override;
  base::Time GetAutoReauthnEmbargoStartTime(
      const url::Origin& relying_party_embedder) override;
  void RecordDisplayAndEmbargo(
      const url::Origin& relying_party_embedder) override;

 private:
  const raw_ptr<HostContentSettingsMap> host_content_settings_map_;
  const raw_ptr<permissions::PermissionDecisionAutoBlocker, DanglingUntriaged>
      permission_autoblocker_;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_AUTO_REAUTHN_PERMISSION_CONTEXT_H_
