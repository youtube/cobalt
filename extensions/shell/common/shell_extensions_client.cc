// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_extensions_client.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/lazy_instance.h"
#include "base/notreached.h"
#include "components/version_info/version_info.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/core_extensions_api_provider.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permission_message_provider.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/shell/common/shell_extensions_api_provider.h"

namespace extensions {

namespace {

// TODO(jamescook): Refactor ChromePermissionsMessageProvider so we can share
// code. For now, this implementation does nothing.
class ShellPermissionMessageProvider : public PermissionMessageProvider {
 public:
  ShellPermissionMessageProvider() = default;
  ShellPermissionMessageProvider(const ShellPermissionMessageProvider&) =
      delete;
  ShellPermissionMessageProvider& operator=(
      const ShellPermissionMessageProvider&) = delete;
  ~ShellPermissionMessageProvider() override = default;

  // PermissionMessageProvider implementation.
  PermissionMessages GetPermissionMessages(
      const PermissionIDSet& permissions) const override {
    return PermissionMessages();
  }

  bool IsPrivilegeIncrease(const PermissionSet& granted_permissions,
                           const PermissionSet& requested_permissions,
                           Manifest::Type extension_type) const override {
    // Ensure we implement this before shipping.
    CHECK(false);
    return false;
  }

  PermissionIDSet GetAllPermissionIDs(
      const PermissionSet& permissions,
      Manifest::Type extension_type) const override {
    return PermissionIDSet();
  }
};

base::LazyInstance<ShellPermissionMessageProvider>::DestructorAtExit
    g_permission_message_provider = LAZY_INSTANCE_INITIALIZER;

}  // namespace

ShellExtensionsClient::ShellExtensionsClient()
    : webstore_base_url_(extension_urls::kChromeWebstoreBaseURL),
      new_webstore_base_url_(extension_urls::kNewChromeWebstoreBaseURL),
      webstore_update_url_(extension_urls::kChromeWebstoreUpdateURL) {
  AddAPIProvider(std::make_unique<CoreExtensionsAPIProvider>());
  AddAPIProvider(std::make_unique<ShellExtensionsAPIProvider>());
}

ShellExtensionsClient::~ShellExtensionsClient() {
}

void ShellExtensionsClient::Initialize() {
  // TODO(jamescook): Do we need to whitelist any extensions?
}

void ShellExtensionsClient::InitializeWebStoreUrls(
    base::CommandLine* command_line) {}

const PermissionMessageProvider&
ShellExtensionsClient::GetPermissionMessageProvider() const {
  NOTIMPLEMENTED();
  return g_permission_message_provider.Get();
}

const std::string ShellExtensionsClient::GetProductName() {
  return "app_shell";
}

void ShellExtensionsClient::FilterHostPermissions(
    const URLPatternSet& hosts,
    URLPatternSet* new_hosts,
    PermissionIDSet* permissions) const {
  NOTIMPLEMENTED();
}

void ShellExtensionsClient::SetScriptingAllowlist(
    const ScriptingAllowlist& allowlist) {
  scripting_allowlist_ = allowlist;
}

const ExtensionsClient::ScriptingAllowlist&
ShellExtensionsClient::GetScriptingAllowlist() const {
  // TODO(jamescook): Real allowlist.
  return scripting_allowlist_;
}

URLPatternSet ShellExtensionsClient::GetPermittedChromeSchemeHosts(
    const Extension* extension,
    const APIPermissionSet& api_permissions) const {
  NOTIMPLEMENTED();
  return URLPatternSet();
}

bool ShellExtensionsClient::IsScriptableURL(const GURL& url,
                                            std::string* error) const {
  // No restrictions on URLs.
  return true;
}

const GURL& ShellExtensionsClient::GetWebstoreBaseURL() const {
  return webstore_base_url_;
}

const GURL& ShellExtensionsClient::GetNewWebstoreBaseURL() const {
  return new_webstore_base_url_;
}

const GURL& ShellExtensionsClient::GetWebstoreUpdateURL() const {
  return webstore_update_url_;
}

bool ShellExtensionsClient::IsBlocklistUpdateURL(const GURL& url) const {
  // TODO(rockot): Maybe we want to do something else here. For now we accept
  // any URL as a blocklist URL because we don't really care.
  return true;
}

}  // namespace extensions
