// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_

#include <set>
#include <vector>

#include "chrome/common/extensions/permissions/chrome_permission_message_rules.h"
#include "extensions/common/permissions/permission_message_provider.h"

namespace extensions {

// Tested in two places:
// 1. chrome_permission_message_provider_unittest.cc, which is a regular unit
//    test for this class
// 2. chrome/browser/extensions/permission_messages_unittest.cc, which is an
//    integration test that ensures messages are correctly generated for
//    extensions created through the extension system.
class ChromePermissionMessageProvider : public PermissionMessageProvider {
 public:
  ChromePermissionMessageProvider();

  ChromePermissionMessageProvider(const ChromePermissionMessageProvider&) =
      delete;
  ChromePermissionMessageProvider& operator=(
      const ChromePermissionMessageProvider&) = delete;

  ~ChromePermissionMessageProvider() override;

  // PermissionMessageProvider implementation.
  PermissionMessages GetPermissionMessages(
      const PermissionIDSet& permissions) const override;
  bool IsPrivilegeIncrease(const PermissionSet& granted_permissions,
                           const PermissionSet& requested_permissions,
                           Manifest::Type extension_type) const override;
  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override;

  // Returns the permissions IDs which should trigger a warning for the user in
  // chrome://management page.
  PermissionIDSet GetManagementUIPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override;

 private:
  // Adds any permission IDs from API permissions to |permission_ids|.
  void AddAPIPermissions(const PermissionSet& permissions,
                         PermissionIDSet* permission_ids) const;

  // Adds any permission IDs from manifest permissions to |permission_ids|.
  void AddManifestPermissions(const PermissionSet& permissions,
                              PermissionIDSet* permission_ids) const;

  // Adds any permission IDs from host permissions to |permission_ids|.
  void AddHostPermissions(const PermissionSet& permissions,
                          PermissionIDSet* permission_ids,
                          Manifest::Type extension_type) const;

  // Adds the IDs of API permissions which should trigger a warning in
  // chrome://management.
  void AddAPIPermissionsForManagementUIWarning(
      const PermissionSet& permissions,
      PermissionIDSet* permission_ids) const;

  // Adds the IDs of manifest permissions which should trigger a warning in
  // chrome://management.
  void AddManifestPermissionsForManagementUIWarning(
      const PermissionSet& permissions,
      PermissionIDSet* permission_ids) const;

  // Returns true if |requested_permissions| has an elevated API or manifest
  // privilege level compared to |granted_permissions|.
  bool IsAPIOrManifestPrivilegeIncrease(
      const PermissionSet& granted_permissions,
      const PermissionSet& requested_permissions) const;

  // Returns true if |requested_permissions| has more host permissions compared
  // to |granted_permissions|.
  bool IsHostPrivilegeIncrease(const PermissionSet& granted_permissions,
                               const PermissionSet& requested_permissions,
                               Manifest::Type extension_type) const;

  PermissionMessages GetPermissionMessagesHelper(
      const PermissionIDSet& permissions,
      const std::vector<ChromePermissionMessageRule>& rules) const;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_CHROME_PERMISSION_MESSAGE_PROVIDER_H_
