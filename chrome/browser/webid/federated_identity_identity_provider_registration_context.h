// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION_CONTEXT_H_
#define CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION_CONTEXT_H_

#include "components/permissions/object_permission_context_base.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/origin.h"

#include <string>
#include <vector>

namespace content {
class BrowserContext;
}

// Context for storing which IdPs the user explicitly accepted.
class FederatedIdentityIdentityProviderRegistrationContext
    : public permissions::ObjectPermissionContextBase {
 public:
  explicit FederatedIdentityIdentityProviderRegistrationContext(
      content::BrowserContext* browser_context);

  FederatedIdentityIdentityProviderRegistrationContext(
      const FederatedIdentityIdentityProviderRegistrationContext&) = delete;
  FederatedIdentityIdentityProviderRegistrationContext& operator=(
      const FederatedIdentityIdentityProviderRegistrationContext&) = delete;

  std::vector<GURL> GetRegisteredIdPs();
  void RegisterIdP(const GURL& origin);
  void UnregisterIdP(const GURL& origin);

  // permissions::ObjectPermissionContextBase:
  std::string GetKeyForObject(const base::Value& object) override;

 private:
  // permissions::ObjectPermissionContextBase:
  bool IsValidObject(const base::Value& object) override;
  std::u16string GetObjectDisplayName(const base::Value& object) override;
};

#endif  // CHROME_BROWSER_WEBID_FEDERATED_IDENTITY_IDENTITY_PROVIDER_REGISTRATION_CONTEXT_H_
