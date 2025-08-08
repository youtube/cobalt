// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/crx_file/id_util.h"
#include "content/public/common/socket_permission_request.h"
#include "extensions/common/constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permission_message_test_util.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/permissions/socket_permission.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern_set.h"
#include "extensions/common/user_script.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::UTF16ToUTF8;
using content::SocketPermissionRequest;
using extension_test_util::LoadManifest;
using extension_test_util::LoadManifestStrict;
using extension_test_util::LoadManifestUnchecked;
using extensions::mojom::APIPermissionID;
using extensions::mojom::ManifestLocation;

namespace extensions {

namespace {

const char kAllHostsPermission[] = "*://*/*";

const char kChromeUntrustedURL[] = "chrome-untrusted://test/index.html";

GURL GetFaviconURL(const char* path) {
  GURL::Replacements replace_path;
  replace_path.SetPathStr(path);
  return GURL(chrome::kChromeUIFaviconURL).ReplaceComponents(replace_path);
}

bool CheckSocketPermission(scoped_refptr<Extension> extension,
                           SocketPermissionRequest::OperationType type,
                           const char* host,
                           uint16_t port) {
  SocketPermission::CheckParam param(type, host, port);
  return extension->permissions_data()->CheckAPIPermissionWithParam(
      APIPermissionID::kSocket, &param);
}

// Creates and returns an extension with the given |id|, |host_permissions|, and
// manifest |location|.
scoped_refptr<const Extension> GetExtensionWithHostPermission(
    const std::string& id,
    const std::string& host_permissions,
    ManifestLocation location) {
  base::Value::List permissions;
  if (!host_permissions.empty())
    permissions.Append(host_permissions);

  return ExtensionBuilder()
      .SetManifest(base::Value::Dict()
                       .Set("name", id)
                       .Set("description", "an extension")
                       .Set("manifest_version", 2)
                       .Set("version", "1.0.0")
                       .Set("permissions", std::move(permissions)))
      .SetLocation(location)
      .SetID(id)
      .Build();
}

// Checks that urls are properly restricted for the given extension.
void CheckRestrictedUrls(const Extension* extension,
                         bool block_chrome_urls,
                         bool block_chrome_untrusted_urls) {
  // We log the name so we know _which_ extension failed here.
  const std::string& name = extension->name();
  const GURL chrome_settings_url(chrome::kChromeUISettingsURL);
  const GURL chrome_extension_url("chrome-extension://foo/bar.html");
  const GURL google_url("https://www.google.com/");
  const GURL self_url("chrome-extension://" + extension->id() + "/foo.html");
  const GURL invalid_url("chrome-debugger://foo/bar.html");
  const GURL chrome_untrusted_url(kChromeUntrustedURL);

  std::string error;
  EXPECT_EQ(block_chrome_urls, extension->permissions_data()->IsRestrictedUrl(
                                   chrome_settings_url, &error))
      << name;
  if (block_chrome_urls)
    EXPECT_EQ(manifest_errors::kCannotAccessChromeUrl, error) << name;
  else
    EXPECT_TRUE(error.empty()) << name;

  error.clear();
  EXPECT_EQ(block_chrome_urls, extension->permissions_data()->IsRestrictedUrl(
                                   chrome_extension_url, &error))
      << name;
  if (block_chrome_urls)
    EXPECT_EQ(manifest_errors::kCannotAccessExtensionUrl, error) << name;
  else
    EXPECT_TRUE(error.empty()) << name;

  EXPECT_EQ(block_chrome_untrusted_urls,
            extension->permissions_data()->IsRestrictedUrl(chrome_untrusted_url,
                                                           &error));
  if (block_chrome_untrusted_urls)
    EXPECT_EQ(manifest_errors::kCannotAccessPage, error) << name;
  else
    EXPECT_TRUE(error.empty()) << name;

  // Google should never be a restricted url.
  error.clear();
  EXPECT_FALSE(
      extension->permissions_data()->IsRestrictedUrl(google_url, &error))
      << name;
  EXPECT_TRUE(error.empty()) << name;

  // We should always be able to access our own extension pages.
  error.clear();
  EXPECT_FALSE(extension->permissions_data()->IsRestrictedUrl(self_url, &error))
      << name;
  EXPECT_TRUE(error.empty()) << name;

  // We should only allow other schemes for extensions when it's an allowed
  // extension.
  error.clear();
  bool allow_on_other_schemes = PermissionsData::CanExecuteScriptEverywhere(
      extension->id(), extension->location());
  EXPECT_EQ(!allow_on_other_schemes,
            extension->permissions_data()->IsRestrictedUrl(invalid_url, &error))
      << name;
  if (!allow_on_other_schemes) {
    EXPECT_EQ(manifest_errors::kCannotAccessPage, error) << name;
  } else {
    EXPECT_TRUE(error.empty());
  }
}

}  // namespace

// NOTE: These tests run in Chrome's unit_tests suite because they depend on
// extension manifest keys (like "content_scripts") that do not exist yet in the
// src/extensions module.
TEST(PermissionsDataTest, EffectiveHostPermissions) {
  URLPatternSet hosts;

  scoped_refptr<Extension> extension =
      LoadManifest("effective_host_permissions", "empty.json");
  EXPECT_EQ(0u, extension->permissions_data()
                    ->GetEffectiveHostPermissions()
                    .patterns()
                    .size());
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "one_host.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://www.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "one_host_wildcard.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://foo.google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "two_hosts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "https_not_considered.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://google.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions",
                           "two_content_scripts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://google.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(extension->permissions_data()
                  ->active_permissions()
                  .HasEffectiveAccessToURL(GURL("http://www.reddit.com")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://news.ycombinator.com")));
  EXPECT_TRUE(
      extension->permissions_data()
          ->active_permissions()
          .HasEffectiveAccessToURL(GURL("http://news.ycombinator.com")));
  EXPECT_FALSE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_FALSE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts2.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  extension = LoadManifest("effective_host_permissions", "all_hosts3.json");
  hosts = extension->permissions_data()->GetEffectiveHostPermissions();
  EXPECT_FALSE(hosts.MatchesURL(GURL("http://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("https://test/")));
  EXPECT_TRUE(hosts.MatchesURL(GURL("http://www.google.com")));
  EXPECT_TRUE(extension->permissions_data()->HasEffectiveAccessToAllHosts());

  // Tab-specific permissions should always be included.
  GURL tab_url("http://www.example.com/");
  {
    URLPatternSet new_hosts;
    new_hosts.AddOrigin(URLPattern::SCHEME_ALL, tab_url);
    extension->permissions_data()->UpdateTabSpecificPermissions(
        1, PermissionSet(APIPermissionSet(), ManifestPermissionSet(),
                         std::move(new_hosts), URLPatternSet()));
  }
  EXPECT_TRUE(
      extension->permissions_data()->GetEffectiveHostPermissions().MatchesURL(
          tab_url));
  extension->permissions_data()->ClearTabSpecificPermissions(1);
  EXPECT_FALSE(
      extension->permissions_data()->GetEffectiveHostPermissions().MatchesURL(
          tab_url));

  extension->permissions_data()->ClearTabSpecificPermissions(1);
  EXPECT_FALSE(
      extension->permissions_data()->GetEffectiveHostPermissions().MatchesURL(
          tab_url));
}

TEST(PermissionsDataTest, SocketPermissions) {
  std::string error;

  scoped_refptr<Extension> extension =
      LoadManifest("socket_permissions", "empty.json");
  EXPECT_FALSE(CheckSocketPermission(
      extension, SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));

  extension = LoadManifestUnchecked("socket_permissions", "socket1.json",
                                    mojom::ManifestLocation::kInternal,
                                    Extension::NO_FLAGS, &error);
  EXPECT_TRUE(extension.get() == nullptr);
  std::string expected_error_msg_header = ErrorUtils::FormatErrorMessage(
      manifest_errors::kInvalidPermissionWithDetail,
      "socket",
      "NULL or empty permission list");
  EXPECT_EQ(expected_error_msg_header, error);

  extension = LoadManifest("socket_permissions", "socket2.json");
  EXPECT_TRUE(CheckSocketPermission(
      extension, SocketPermissionRequest::TCP_CONNECT, "www.example.com", 80));
  EXPECT_FALSE(CheckSocketPermission(
      extension, SocketPermissionRequest::UDP_BIND, "", 80));
  EXPECT_TRUE(CheckSocketPermission(
      extension, SocketPermissionRequest::UDP_BIND, "", 8888));

  EXPECT_FALSE(CheckSocketPermission(
      extension, SocketPermissionRequest::UDP_SEND_TO, "example.com", 1900));
  EXPECT_TRUE(CheckSocketPermission(extension,
                                    SocketPermissionRequest::UDP_SEND_TO,
                                    "239.255.255.250", 1900));
}

TEST(PermissionsDataTest, IsRestrictedUrl) {
  scoped_refptr<const Extension> extension = GetExtensionWithHostPermission(
      "normal_extension", kAllHostsPermission, ManifestLocation::kInternal);
  // Chrome and chrome-untrusted:// urls should be blocked for normal
  // extensions.
  CheckRestrictedUrls(extension.get(), /*block_chrome_urls=*/true,
                      /*block_chrome_untrusted_urls=*/true);

  scoped_refptr<const Extension> component = GetExtensionWithHostPermission(
      "component", kAllHostsPermission, ManifestLocation::kComponent);
  // Chrome and chrome-untrusted:// urls should be accessible by component
  // extensions.
  CheckRestrictedUrls(component.get(), /*block_chrome_urls=*/false,
                      /*block_chrome_untrusted_urls=*/false);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);
  // Enabling the switch should allow all extensions to access chrome urls but
  // not chrome-untrusted:// urls.
  CheckRestrictedUrls(extension.get(), /*block_chrome_urls=*/false,
                      /*block_chrome_untrusted_urls=*/true);
}

TEST(PermissionsDataTest, GetPermissionMessages_ManyAPIPermissions) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "many-apis.json");
  // Warning for "tabs" is suppressed by "history" permission.
  std::vector<std::string> expected_messages;
  expected_messages.push_back("Read and change your data on api.flickr.com");
  expected_messages.push_back("Read and change your bookmarks");
  expected_messages.push_back("Detect your physical location");
  expected_messages.push_back(
      "Read and change your browsing history on all your signed-in devices");
  expected_messages.push_back("Manage your apps, extensions, and themes");
  EXPECT_TRUE(VerifyPermissionMessages(extension->permissions_data(),
                                       expected_messages, false));
}

TEST(PermissionsDataTest, GetPermissionMessages_ManyHostsPermissions) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "more-than-3-hosts.json");
  std::vector<std::string> submessages;
  submessages.push_back("www.a.com");
  submessages.push_back("www.b.com");
  submessages.push_back("www.c.com");
  submessages.push_back("www.d.com");
  submessages.push_back("www.e.com");
  EXPECT_TRUE(VerifyOnePermissionMessageWithSubmessages(
      extension->permissions_data(),
      "Read and change your data on a number of websites", submessages));
}

TEST(PermissionsDataTest, GetPermissionMessages_ManyHosts) {
  scoped_refptr<Extension> extension =
      LoadManifest("permissions", "many-hosts.json");
  EXPECT_TRUE(VerifyOnePermissionMessage(
      extension->permissions_data(),
      "Read and change your data on encrypted.google.com and www.google.com"));
}

TEST(PermissionsDataTest, ExtensionScheme) {
  GURL external_file(
      "chrome-extension://abcdefghijklmnopabcdefghijklmnop/index.html");

  // A regular extension shouldn't get access to chrome-extension: scheme URLs
  // even with <all_urls> specified.
  scoped_refptr<const Extension> extension = GetExtensionWithHostPermission(
      "regular_extension", "<all_urls>", ManifestLocation::kUnpacked);
  ASSERT_FALSE(extension->permissions_data()->HasHostPermission(external_file));

  // Component extensions should get access to chrome-extension: scheme URLs
  // when <all_urls> is specified.
  extension = GetExtensionWithHostPermission(
      "component_extension", "<all_urls>", ManifestLocation::kComponent);
  ASSERT_TRUE(extension->permissions_data()->HasHostPermission(external_file));
}

// Base class for testing the CanAccessPage and CanCaptureVisiblePage
// methods of Extension for extensions with various permissions.
class ExtensionScriptAndCaptureVisibleTest : public testing::Test {
 protected:
  ExtensionScriptAndCaptureVisibleTest()
      : http_url("http://www.google.com"),
        http_url_with_path("http://www.google.com/index.html"),
        https_url("https://www.google.com"),
        example_com("https://example.com"),
        test_example_com("https://test.example.com"),
        sample_example_com("https://sample.example.com"),
        file_url("file:///foo/bar"),
        favicon_url(GetFaviconURL("http://www.google.com")),
        extension_url("chrome-extension://" +
                      crx_file::id_util::GenerateIdForPath(
                          base::FilePath(FILE_PATH_LITERAL("foo")))),
        settings_url(chrome::kChromeUISettingsURL),
        chrome_untrusted_url(kChromeUntrustedURL),
        about_flags_url("about:flags") {
    urls_.insert(http_url);
    urls_.insert(http_url_with_path);
    urls_.insert(https_url);
    urls_.insert(example_com);
    urls_.insert(test_example_com);
    urls_.insert(sample_example_com);
    urls_.insert(file_url);
    urls_.insert(favicon_url);
    urls_.insert(extension_url);
    urls_.insert(settings_url);
    urls_.insert(chrome_untrusted_url);
    urls_.insert(about_flags_url);
    // Ignore the policy delegate for this test.
    PermissionsData::SetPolicyDelegate(nullptr);
  }

  enum AccessType {
    DISALLOWED,
    ALLOWED_SCRIPT_ONLY,
    ALLOWED_CAPTURE_ONLY,
    ALLOWED_SCRIPT_AND_CAPTURE,
  };

  bool IsAllowedScript(const Extension* extension, const GURL& url) {
    return IsAllowedScript(extension, url, -1);
  }

  AccessType GetExtensionAccess(const Extension* extension,
                                const GURL& url,
                                int tab_id) {
    bool allowed_script = IsAllowedScript(extension, url, tab_id);
    bool allowed_capture = extension->permissions_data()->CanCaptureVisiblePage(
        url, tab_id, nullptr,
        extensions::CaptureRequirement::kActiveTabOrAllUrls);

    if (allowed_script && allowed_capture)
      return ALLOWED_SCRIPT_AND_CAPTURE;
    if (allowed_script)
      return ALLOWED_SCRIPT_ONLY;
    if (allowed_capture)
      return ALLOWED_CAPTURE_ONLY;
    return DISALLOWED;
  }

  AccessType GetExtensionAccess(const Extension* extension, const GURL& url) {
    return GetExtensionAccess(extension, url, -1);
  }

  testing::AssertionResult ScriptAllowedExclusivelyOnTab(
      const Extension* extension,
      const std::set<GURL>& allowed_urls,
      int tab_id) {
    std::vector<std::string> errors;
    for (const GURL& url : urls_) {
      AccessType access = GetExtensionAccess(extension, url, tab_id);
      AccessType expected_access =
          allowed_urls.count(url) ? ALLOWED_SCRIPT_ONLY : DISALLOWED;
      if (access != expected_access) {
        errors.push_back(
            base::StringPrintf("Error for url '%s': expected %d, found %d",
                               url.spec().c_str(), expected_access, access));
      }
    }

    if (!errors.empty())
      return testing::AssertionFailure() << base::JoinString(errors, "\n");
    return testing::AssertionSuccess();
  }

  // URLs that are "safe" to provide scripting and capture visible tab access
  // to if the permissions allow it.
  const GURL http_url;
  const GURL http_url_with_path;
  const GURL https_url;
  const GURL example_com;
  const GURL test_example_com;
  const GURL sample_example_com;
  const GURL file_url;

  // We should allow host permission but not scripting permission for favicon
  // urls.
  const GURL favicon_url;

  // URLs that regular extensions should never get access to.
  const GURL extension_url;
  const GURL settings_url;
  const GURL chrome_untrusted_url;
  const GURL about_flags_url;

 private:
  bool IsAllowedScript(const Extension* extension,
                       const GURL& url,
                       int tab_id) {
    return extension->permissions_data()->CanAccessPage(url, tab_id, nullptr);
  }

  // The set of all URLs above.
  std::set<GURL> urls_;
};

TEST_F(ExtensionScriptAndCaptureVisibleTest, Permissions) {
  // Test <all_urls> for regular extensions.
  scoped_refptr<Extension> extension = LoadManifestStrict("script_and_capture",
      "extension_regular_all.json");

  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  // Note: this uses IsAllowedScript() (instead of GetExtensionAccess()) because
  // they are theoretically testing iframed content, and capturing (just)
  // iframed content doesn't make sense. It might be nice to more completely
  // enforce that this is for subframe access, though.
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url));
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url_with_path));
  EXPECT_TRUE(IsAllowedScript(extension.get(), https_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), within_extension_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), extension_url));

  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
  EXPECT_FALSE(
      extension->permissions_data()->HasHostPermission(chrome_untrusted_url));
  EXPECT_FALSE(
      extension->permissions_data()->HasHostPermission(about_flags_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension = LoadManifestStrict("script_and_capture",
      "extension_wildcard.json");
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  extension =
      LoadManifest("script_and_capture", "extension_wildcard_settings.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));

  // Having chrome://*/ should not work for regular extensions. Note that
  // for favicon access, we require the explicit pattern chrome://favicon/*.
  std::string error;
  extension = LoadManifestUnchecked(
      "script_and_capture", "extension_wildcard_chrome.json",
      mojom::ManifestLocation::kInternal, Extension::NO_FLAGS, &error);
  const std::vector<InstallWarning>& warnings = extension->install_warnings();
  EXPECT_FALSE(warnings.empty());
  EXPECT_EQ(ErrorUtils::FormatErrorMessage(
                manifest_errors::kInvalidPermissionScheme,
                manifest_keys::kPermissions, "chrome://*/"),
            warnings[0].message);
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));

  // Having chrome://favicon/* should not give you chrome://*
  extension = LoadManifestStrict("script_and_capture",
      "extension_chrome_favicon_wildcard.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));

  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Having http://favicon should not give you chrome://favicon
  extension = LoadManifestStrict("script_and_capture",
      "extension_http_favicon.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));

  // Component extensions with <all_urls> should get everything except for
  // "chrome" scheme URLs.
  extension =
      LoadManifest("script_and_capture", "extension_component_all.json",
                   mojom::ManifestLocation::kComponent, Extension::NO_FLAGS);
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));

  // Component extensions should only get access to what they ask for.
  extension =
      LoadManifest("script_and_capture", "extension_component_google.json",
                   mojom::ManifestLocation::kComponent, Extension::NO_FLAGS);
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), extension_url));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, PermissionsWithChromeURLsEnabled) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kExtensionsOnChromeURLs);

  // Test <all_urls> for regular extensions.
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "extension_regular_all.json");
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(
      ALLOWED_SCRIPT_AND_CAPTURE,
      GetExtensionAccess(extension.get(), favicon_url));  // chrome:// requested
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url));
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url_with_path));
  EXPECT_TRUE(IsAllowedScript(extension.get(), https_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), within_extension_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), extension_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), chrome_untrusted_url));

  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_FALSE(permissions_data->HasHostPermission(settings_url));
  EXPECT_FALSE(permissions_data->HasHostPermission(chrome_untrusted_url));
  EXPECT_FALSE(permissions_data->HasHostPermission(about_flags_url));
  EXPECT_TRUE(permissions_data->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension =
      LoadManifestStrict("script_and_capture", "extension_wildcard.json");
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  extension =
      LoadManifest("script_and_capture", "extension_wildcard_settings.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));

  // Having chrome://*/ should work for regular extensions with the flag
  // enabled.
  std::string error;
  extension = LoadManifestUnchecked(
      "script_and_capture", "extension_wildcard_chrome.json",
      mojom::ManifestLocation::kInternal, Extension::NO_FLAGS, &error);
  EXPECT_FALSE(extension.get() == nullptr);
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), favicon_url));

  // Having chrome://favicon/* should not give you chrome://*
  extension = LoadManifestStrict("script_and_capture",
                                 "extension_chrome_favicon_wildcard.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Having http://favicon should not give you chrome://favicon
  extension =
      LoadManifestStrict("script_and_capture", "extension_http_favicon.json");
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));

  // Component extensions with <all_urls> should get everything except for
  // "chrome" scheme URLs.
  extension =
      LoadManifest("script_and_capture", "extension_component_all.json",
                   mojom::ManifestLocation::kComponent, Extension::NO_FLAGS);
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));

  // Component extensions should only get access to what they ask for.
  extension =
      LoadManifest("script_and_capture", "extension_component_google.json",
                   mojom::ManifestLocation::kComponent, Extension::NO_FLAGS);
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), chrome_untrusted_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), extension_url));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, TabSpecific) {
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "tab_specific.json");

  const PermissionsData* permissions_data = extension->permissions_data();
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(1));
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(2));

  std::set<GURL> no_urls;

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  URLPatternSet allowed_hosts;
  allowed_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL,
                                      http_url.spec()));
  std::set<GURL> allowed_urls;
  allowed_urls.insert(http_url);
  // http_url_with_path() will also be allowed, because Extension should be
  // considering the security origin of the URL not the URL itself, and
  // http_url is in allowed_hosts.
  allowed_urls.insert(http_url_with_path);

  {
    PermissionSet permissions(APIPermissionSet(), ManifestPermissionSet(),
                              allowed_hosts.Clone(), URLPatternSet());
    permissions_data->UpdateTabSpecificPermissions(0, permissions);
    EXPECT_EQ(permissions.explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(0)
                  ->explicit_hosts());
  }

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), allowed_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  std::set<GURL> more_allowed_urls = allowed_urls;
  more_allowed_urls.insert(https_url);
  more_allowed_urls.insert(file_url);
  URLPatternSet more_allowed_hosts = allowed_hosts.Clone();
  more_allowed_hosts.AddPattern(URLPattern(URLPattern::SCHEME_ALL,
                                           https_url.spec()));
  more_allowed_hosts.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, file_url.spec()));

  {
    PermissionSet permissions1(APIPermissionSet(), ManifestPermissionSet(),
                               allowed_hosts.Clone(), URLPatternSet());
    permissions_data->UpdateTabSpecificPermissions(0, permissions1);
    EXPECT_EQ(permissions1.explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(0)
                  ->explicit_hosts());

    PermissionSet permissions2(APIPermissionSet(), ManifestPermissionSet(),
                               more_allowed_hosts.Clone(), URLPatternSet());
    permissions_data->UpdateTabSpecificPermissions(1, permissions2);
    EXPECT_EQ(permissions2.explicit_hosts(),
              permissions_data->GetTabSpecificPermissionsForTesting(1)
                  ->explicit_hosts());
  }

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), allowed_urls, 0));
  EXPECT_TRUE(
      ScriptAllowedExclusivelyOnTab(extension.get(), more_allowed_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(0);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(0));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(
      ScriptAllowedExclusivelyOnTab(extension.get(), more_allowed_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));

  permissions_data->ClearTabSpecificPermissions(1);
  EXPECT_FALSE(permissions_data->GetTabSpecificPermissionsForTesting(1));

  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 0));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 1));
  EXPECT_TRUE(ScriptAllowedExclusivelyOnTab(extension.get(), no_urls, 2));
}

// Test that activeTab is required for capturing chrome:// urls with
// tabs.captureVisibleTab. https://crbug.com/810220.
TEST_F(ExtensionScriptAndCaptureVisibleTest, CaptureChromeURLs) {
  const int kTabId = 42;
  scoped_refptr<const Extension> all_urls =
      ExtensionBuilder("all urls").AddPermission("<all_urls>").Build();
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(all_urls.get(), settings_url, kTabId));

  scoped_refptr<const Extension> active_tab =
      ExtensionBuilder("active tab").AddPermission("activeTab").Build();
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(active_tab.get(), settings_url, kTabId));
  {
    APIPermissionSet tab_api_permissions;
    tab_api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet tab_hosts;
    tab_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(),
                        settings_url.DeprecatedGetOriginAsURL());
    PermissionSet tab_permissions(std::move(tab_api_permissions),
                                  ManifestPermissionSet(), tab_hosts.Clone(),
                                  tab_hosts.Clone());
    active_tab->permissions_data()->UpdateTabSpecificPermissions(
        kTabId, tab_permissions);
  }
  EXPECT_EQ(ALLOWED_CAPTURE_ONLY,
            GetExtensionAccess(active_tab.get(), settings_url, kTabId));
}

// chrome-untrusted:// can never be captured.
TEST_F(ExtensionScriptAndCaptureVisibleTest, CaptureChromeUntrustedURLs) {
  const int kTabId = 42;
  scoped_refptr<const Extension> all_urls =
      ExtensionBuilder("all urls").AddPermission("<all_urls>").Build();
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(all_urls.get(), chrome_untrusted_url, kTabId));

  scoped_refptr<const Extension> active_tab =
      ExtensionBuilder("active tab").AddPermission("activeTab").Build();
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(active_tab.get(), chrome_untrusted_url, kTabId));

  {
    APIPermissionSet tab_api_permissions;
    tab_api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet tab_hosts;
    // Even extensions that can execute scripts everywhere, e.g. component
    // extensions, are not able to capture chrome-untrusted://.
    tab_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(
                            /*can_execute_script_everywhere=*/true),
                        chrome_untrusted_url.DeprecatedGetOriginAsURL());
    PermissionSet tab_permissions(std::move(tab_api_permissions),
                                  ManifestPermissionSet(), tab_hosts.Clone(),
                                  tab_hosts.Clone());
    active_tab->permissions_data()->UpdateTabSpecificPermissions(
        kTabId, tab_permissions);
  }
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(active_tab.get(), chrome_untrusted_url, kTabId));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, CaptureFileURLs) {
  const int kTabId = 42;
  scoped_refptr<const Extension> all_urls =
      ExtensionBuilder("all urls").AddPermission("<all_urls>").Build();
  // Currently, the extension has not been granted file access, so it should
  // not have access to a file:// URL.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(all_urls.get(), file_url, kTabId));

  scoped_refptr<const Extension> active_tab =
      ExtensionBuilder("active tab").AddPermission("activeTab").Build();
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(active_tab.get(), file_url, kTabId));
  {
    APIPermissionSet tab_api_permissions;
    tab_api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet tab_hosts;
    tab_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(),
                        file_url.DeprecatedGetOriginAsURL());
    PermissionSet tab_permissions(std::move(tab_api_permissions),
                                  ManifestPermissionSet(), tab_hosts.Clone(),
                                  tab_hosts.Clone());
    active_tab->permissions_data()->UpdateTabSpecificPermissions(
        kTabId, tab_permissions);
  }
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(active_tab.get(), file_url, kTabId));
}

// Check that both the old and new webstore URL are inaccessible.
TEST(PermissionsDataTest, ChromeWebstoreUrl) {
  scoped_refptr<const Extension> normal_extension =
      GetExtensionWithHostPermission("all_hosts_normal_extension",
                                     kAllHostsPermission,
                                     ManifestLocation::kInternal);
  scoped_refptr<const Extension> policy_extension =
      GetExtensionWithHostPermission("all_hosts_policy_extension",
                                     kAllHostsPermission,
                                     ManifestLocation::kExternalPolicy);
  scoped_refptr<const Extension> unpacked_extension =
      GetExtensionWithHostPermission("all_hosts_unpacked_extension",
                                     kAllHostsPermission,
                                     ManifestLocation::kUnpacked);
  const Extension* extensions[] = {
      normal_extension.get(), policy_extension.get(), unpacked_extension.get(),
  };
  const GURL kWebstoreUrls[] = {
      GURL("https://chrome.google.com/webstore"),
      GURL("https://chrome.google.com./webstore"),
      GURL("https://chrome.google.com/webstore/category/extensions"),
      GURL("https://chrome.google.com./webstore/category/extensions"),
      GURL("https://chrome.google.com/webstore/search/foo"),
      GURL("https://chrome.google.com./webstore/search/foo"),
      GURL("https://chrome.google.com/webstore/detail/"
           "empty-new-tab-page/dpjamkmjmigaoobjbekmfgabipmfilij"),
      GURL("https://chrome.google.com./webstore/detail/"
           "empty-new-tab-page/dpjamkmjmigaoobjbekmfgabipmfilij"),
      GURL("https://chromewebstore.google.com"),
      GURL("https://chromewebstore.google.com."),
      GURL("https://chromewebstore.google.com/category/foo/bar"),
      GURL("https://chromewebstore.google.com./category/foo/bar"),
      GURL("https://chromewebstore.google.com/collection/foo"),
      GURL("https://chromewebstore.google.com./collection/foo"),
      GURL("https://chromewebstore.google.com/detail/"
           "empty-new-tab-page/dpjamkmjmigaoobjbekmfgabipmfilij"),
      GURL("https://chromewebstore.google.com/search/foo"),
      GURL("https://chromewebstore.google.com/user/installed"),
      GURL("https://chromewebstore.google.com/user/library"),
      GURL("https://chromewebstore.google.com/report/"
           "dpjamkmjmigaoobjbekmfgabipmfilij"),

      // Unintuitively, the script blocking also applies to deeper subdomains
      // and other paths on chrome.google.com.
      // TODO(crbug.com/1355623): We probably want to adjust the logic so these
      // are not the case, but it's better to have the current behavior
      // explicitly documented in tests for now.
      GURL("https://foo.chrome.google.com/webstore"),
      GURL("https://foo.bar.chromewebstore.google.com"),
      GURL("https://chrome.google.com/foo"),
  };

  const int kTabId = 1;
  std::string error;
  URLPatternSet tab_hosts;
  tab_hosts.AddOrigin(
      UserScript::ValidUserScriptSchemes(),
      GURL("https://chrome.google.com/webstore").DeprecatedGetOriginAsURL());
  tab_hosts.AddOrigin(
      UserScript::ValidUserScriptSchemes(),
      GURL("https://chrome.google.com./webstore").DeprecatedGetOriginAsURL());
  tab_hosts.AddOrigin(
      UserScript::ValidUserScriptSchemes(),
      GURL("https://chromewebstore.google.com/").DeprecatedGetOriginAsURL());
  tab_hosts.AddOrigin(
      UserScript::ValidUserScriptSchemes(),
      GURL("https://chromewebstore.google.com./").DeprecatedGetOriginAsURL());
  PermissionSet tab_permissions(APIPermissionSet(), ManifestPermissionSet(),
                                tab_hosts.Clone(), tab_hosts.Clone());
  for (const Extension* extension : extensions) {
    // Give the extension activeTab permissions to run on the webstore - it
    // shouldn't make a difference.
    extension->permissions_data()->UpdateTabSpecificPermissions(
        kTabId, tab_permissions);
    for (const GURL& url : kWebstoreUrls) {
      EXPECT_EQ(PermissionsData::PageAccess::kDenied,
                extension->permissions_data()->GetPageAccess(url, -1, &error))
          << extension->name() << ": " << url;
      EXPECT_EQ(PermissionsData::PageAccess::kDenied,
                extension->permissions_data()->GetContentScriptAccess(url, -1,
                                                                      &error))
          << extension->name() << ": " << url;
      EXPECT_EQ(
          PermissionsData::PageAccess::kDenied,
          extension->permissions_data()->GetPageAccess(url, kTabId, &error))
          << extension->name() << ": " << url;
      EXPECT_EQ(PermissionsData::PageAccess::kDenied,
                extension->permissions_data()->GetContentScriptAccess(
                    url, kTabId, &error))
          << extension->name() << ": " << url;
    }
  }
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, PolicyHostRestrictionsSwap) {
  // Makes sure when an extension gets an individual policy for host
  // restrictions it overrides the default policy. Also tests transitioning back
  // to the default policy when an individual policy is removed.
  URLPattern example_com_pattern =
      URLPattern(URLPattern::SCHEME_ALL, "*://*.example.com/*");
  URLPattern test_example_com_pattern =
      URLPattern(URLPattern::SCHEME_ALL, "*://test.example.com/*");
  URLPatternSet default_blocked;
  URLPatternSet default_allowed;
  default_blocked.AddPattern(example_com_pattern);
  default_allowed.AddPattern(test_example_com_pattern);
  int context_id = 7;

  // Test <all_urls> for regular extensions.
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "extension_regular_all.json");
  extension->permissions_data()->SetContextId(context_id);
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  PermissionsData::SetDefaultPolicyHostRestrictions(context_id, default_blocked,
                                                    default_allowed);

  // The default policy applies to all extensions at this point. The extension
  // should be able to access test.example.com but be blocked from
  // accessing any other subdomains of example.com or example.com itself.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));

  URLPatternSet blocked;
  blocked.AddPattern(test_example_com_pattern);
  URLPatternSet allowed;
  extension->permissions_data()->SetPolicyHostRestrictions(blocked, allowed);

  // We've applied an individual policy which overrides the default policy.
  // The only URL that should be blocked is test.example.com.
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), test_example_com));

  blocked.AddPattern(example_com_pattern);
  allowed.AddPattern(test_example_com_pattern);
  extension->permissions_data()->SetPolicyHostRestrictions(blocked, allowed);

  // Adding example.com and all its subdomains to the blocked list and
  // test.example.com to the allowlist. This is still the individual policy
  // Since the allowlist overrides a blocklist we expect to allow access to
  // test.example.com but block access to all other example.com subdomains
  // (sample.example.com) and example.com itself.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));

  blocked.ClearPatterns();
  allowed.ClearPatterns();
  extension->permissions_data()->SetPolicyHostRestrictions(blocked, allowed);

  // Cleared all URLs from the individual policy, so all URLs should have
  // access. We want to make sure that a block at the default level doesn't
  // apply since we're still definining an individual policy.
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));

  // Flip back to using default policy for this extension.
  extension->permissions_data()->SetUsesDefaultHostRestrictions();

  // Make sure the default policy has the same effect as before we defined an
  // individual policy. Access to test.example.com should be allowed, but all
  // other subdomains and example.com itself should be blocked.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));
}

TEST_F(ExtensionScriptAndCaptureVisibleTest, PolicyHostRestrictions) {
  // Test that host restrictions applied by policy take effect on normal URLs,
  // iframe urls, different schemes, and components.
  URLPatternSet default_blocked;
  URLPatternSet default_allowed;
  default_blocked.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "https://*.example.com/*"));
  default_allowed.AddPattern(
      URLPattern(URLPattern::SCHEME_ALL, "https://test.example.com/*"));
  // give the profile some consistent id
  int context_id = 7;

  // In all of these tests, test.example.com should have scripting allowed, with
  // all other subdomains and example.com itself blocked.

  // Test <all_urls> for regular extensions.
  scoped_refptr<Extension> extension =
      LoadManifestStrict("script_and_capture", "extension_regular_all.json");
  extension->permissions_data()->SetContextId(context_id);
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  PermissionsData::SetDefaultPolicyHostRestrictions(context_id, default_blocked,
                                                    default_allowed);

  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), extension_url));

  // Test access to iframed content.
  GURL within_extension_url = extension->GetResourceURL("page.html");
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url));
  EXPECT_TRUE(IsAllowedScript(extension.get(), http_url_with_path));
  EXPECT_FALSE(IsAllowedScript(extension.get(), example_com));
  EXPECT_TRUE(IsAllowedScript(extension.get(), test_example_com));
  EXPECT_FALSE(IsAllowedScript(extension.get(), sample_example_com));
  EXPECT_TRUE(IsAllowedScript(extension.get(), https_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), within_extension_url));
  EXPECT_FALSE(IsAllowedScript(extension.get(), extension_url));

  // Supress host permission for example.com since its on the blocklist
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(example_com));
  // Allow host permission for test.example.com since its on the allowlist and
  // blocklist. The allowlist overrides the blocklist.
  EXPECT_TRUE(
      extension->permissions_data()->HasHostPermission(test_example_com));
  EXPECT_FALSE(extension->permissions_data()->HasHostPermission(settings_url));
  EXPECT_FALSE(
      extension->permissions_data()->HasHostPermission(about_flags_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));

  // Test * for scheme, which implies just the http/https schemes.
  extension =
      LoadManifestStrict("script_and_capture", "extension_wildcard.json");
  extension->permissions_data()->SetContextId(context_id);
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  PermissionsData::SetDefaultPolicyHostRestrictions(context_id, default_blocked,
                                                    default_allowed);
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), test_example_com));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), about_flags_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), file_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), favicon_url));
  extension =
      LoadManifest("script_and_capture", "extension_wildcard_settings.json");
  extension->permissions_data()->SetDefaultPolicyHostRestrictions(
      0, default_blocked, default_allowed);
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));

  // Component extensions with <all_urls> should get everything regardless of
  // policy, except for chrome scheme URLs.
  extension =
      LoadManifest("script_and_capture", "extension_component_all.json",
                   mojom::ManifestLocation::kComponent, Extension::NO_FLAGS);
  extension->permissions_data()->SetContextId(context_id);
  extension->permissions_data()->SetUsesDefaultHostRestrictions();
  PermissionsData::SetDefaultPolicyHostRestrictions(0, default_blocked,
                                                    default_allowed);
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), http_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), https_url));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), test_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), sample_example_com));
  EXPECT_EQ(ALLOWED_SCRIPT_AND_CAPTURE,
            GetExtensionAccess(extension.get(), favicon_url));
  EXPECT_TRUE(extension->permissions_data()->HasHostPermission(favicon_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), settings_url));
}

// Tests that user host restrictions disallow normal extensions from running
// on specified pages.
TEST_F(ExtensionScriptAndCaptureVisibleTest,
       UserHostRestrictions_Restrictions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);
  constexpr int kContextId = 0;
  scoped_refptr<const Extension> extension = GetExtensionWithHostPermission(
      "extension", kAllHostsPermission, ManifestLocation::kInternal);
  extension->permissions_data()->SetContextId(kContextId);
  const GURL first_url("https://first.example");
  const GURL first_url_subdomain("https://subdomain.first.example");
  const GURL second_url("https://second.example");
  const std::string first_domain_pattern = "https://*.first.example/*";

  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), first_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), first_url_subdomain));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), second_url));

  {
    URLPatternSet blocked_urls;
    blocked_urls.AddPattern(URLPattern(Extension::kValidHostPermissionSchemes,
                                       first_domain_pattern));
    PermissionsData::SetUserHostRestrictions(
        kContextId, std::move(blocked_urls), URLPatternSet());
  }

  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), first_url));
  EXPECT_EQ(DISALLOWED,
            GetExtensionAccess(extension.get(), first_url_subdomain));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), second_url));

  {
    URLPatternSet blocked_urls;
    blocked_urls.AddPattern(URLPattern(Extension::kValidHostPermissionSchemes,
                                       first_domain_pattern));
    URLPatternSet allowed_urls;
    allowed_urls.AddOrigin(Extension::kValidHostPermissionSchemes,
                           first_url_subdomain);
    PermissionsData::SetUserHostRestrictions(
        kContextId, std::move(blocked_urls), std::move(allowed_urls));
  }

  // Allowed URLs take precedent over blocked URLs.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), first_url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), first_url_subdomain));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), second_url));
}

// Tests that user host restrictions don't apply to exempt extensions (like
// policy-installed and component extensions).
TEST_F(ExtensionScriptAndCaptureVisibleTest,
       UserHostRestrictions_ExemptExtensions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);

  constexpr int kContextId = 0;
  scoped_refptr<const Extension> internal = GetExtensionWithHostPermission(
      "internal", kAllHostsPermission, ManifestLocation::kInternal);
  scoped_refptr<const Extension> policy = GetExtensionWithHostPermission(
      "policy", kAllHostsPermission, ManifestLocation::kExternalPolicy);
  scoped_refptr<const Extension> component = GetExtensionWithHostPermission(
      "component", kAllHostsPermission, ManifestLocation::kComponent);
  internal->permissions_data()->SetContextId(kContextId);
  policy->permissions_data()->SetContextId(kContextId);
  component->permissions_data()->SetContextId(kContextId);

  const GURL url("https://foo.example");

  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(internal.get(), url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(policy.get(), url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(component.get(), url));

  {
    URLPatternSet blocked_urls;
    blocked_urls.AddOrigin(Extension::kValidHostPermissionSchemes, url);
    PermissionsData::SetUserHostRestrictions(
        kContextId, std::move(blocked_urls), URLPatternSet());
  }

  EXPECT_EQ(DISALLOWED, GetExtensionAccess(internal.get(), url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(policy.get(), url));
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY, GetExtensionAccess(component.get(), url));
}

// Tests that user host restrictions don't let extensions run on URLs that the
// extension doesn't request.
TEST_F(ExtensionScriptAndCaptureVisibleTest,
       UserHostRestrictions_DontGrantUnrequestedPermissions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);

  constexpr int kContextId = 0;
  scoped_refptr<const Extension> internal = GetExtensionWithHostPermission(
      "internal", "https://requested.example/*", ManifestLocation::kInternal);
  internal->permissions_data()->SetContextId(kContextId);

  const GURL unrequested_url("https://unrequested.example");

  EXPECT_EQ(DISALLOWED, GetExtensionAccess(internal.get(), unrequested_url));

  {
    URLPatternSet allowed_urls;
    allowed_urls.AddOrigin(Extension::kValidHostPermissionSchemes,
                           unrequested_url);
    PermissionsData::SetUserHostRestrictions(kContextId, URLPatternSet(),
                                             std::move(allowed_urls));
  }

  EXPECT_EQ(DISALLOWED, GetExtensionAccess(internal.get(), unrequested_url));
}

// Tests that a user specifying an otherwise-restricted URL as "allowed" does
// not allow extensions to run on that URL.
TEST_F(ExtensionScriptAndCaptureVisibleTest,
       UserHostRestrictions_ForbiddenURLs) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      extensions_features::kExtensionsMenuAccessControl);

  constexpr int kContextId = 0;
  scoped_refptr<const Extension> extension = GetExtensionWithHostPermission(
      "extension", kAllHostsPermission, ManifestLocation::kInternal);
  extension->permissions_data()->SetContextId(kContextId);
  const GURL policy_url("https://policy-protected.example");
  const GURL internal_url("chrome://settings");

  // To start, the extension should have access to the policy URL (since we
  // haven't applied policy settings), but not to the chrome:-scheme URL
  // (always restricted).
  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), policy_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), internal_url));

  {
    URLPatternSet blocked_by_policy;
    blocked_by_policy.AddOrigin(Extension::kValidHostPermissionSchemes,
                                policy_url);
    PermissionsData::SetDefaultPolicyHostRestrictions(
        kContextId, blocked_by_policy, URLPatternSet());
  }

  // Applying policy settings should disallow the policy URL.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), policy_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), internal_url));

  {
    URLPatternSet allowed_urls;
    allowed_urls.AddOrigin(Extension::kValidHostPermissionSchemes, policy_url);
    allowed_urls.AddOrigin(Extension::kValidHostPermissionSchemes,
                           internal_url);
    PermissionsData::SetUserHostRestrictions(kContextId, URLPatternSet(),
                                             std::move(allowed_urls));
  }

  // Even if the user says the policy URL is allowed, policies take precedence.
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), policy_url));
  EXPECT_EQ(DISALLOWED, GetExtensionAccess(extension.get(), internal_url));
}

// Tests that user host restrictions are not applied when the corresponding
// feature is disabled.
TEST_F(ExtensionScriptAndCaptureVisibleTest,
       UserHostRestrictions_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      extensions_features::kExtensionsMenuAccessControl);
  constexpr int kContextId = 0;
  scoped_refptr<const Extension> extension = GetExtensionWithHostPermission(
      "extension", kAllHostsPermission, ManifestLocation::kInternal);
  extension->permissions_data()->SetContextId(kContextId);
  const GURL restricted_url("https://first.example");

  {
    URLPatternSet blocked_urls;
    blocked_urls.AddOrigin(Extension::kValidHostPermissionSchemes,
                           restricted_url);
    PermissionsData::SetUserHostRestrictions(
        kContextId, std::move(blocked_urls), URLPatternSet());
  }

  EXPECT_EQ(ALLOWED_SCRIPT_ONLY,
            GetExtensionAccess(extension.get(), restricted_url));
}

class CaptureVisiblePageTest : public testing::Test {
 public:
  CaptureVisiblePageTest() = default;

  CaptureVisiblePageTest(const CaptureVisiblePageTest&) = delete;
  CaptureVisiblePageTest& operator=(const CaptureVisiblePageTest&) = delete;

  ~CaptureVisiblePageTest() override = default;

  bool CanCapture(const Extension& extension,
                  const GURL& url,
                  extensions::CaptureRequirement capture_requirement) {
    return extension.permissions_data()->CanCaptureVisiblePage(
        url, kTabId, nullptr /*error*/, capture_requirement);
  }

  void GrantActiveTab(const Extension& extension, const GURL& url) {
    APIPermissionSet tab_api_permissions;
    tab_api_permissions.insert(APIPermissionID::kTab);
    URLPatternSet tab_hosts;
    tab_hosts.AddOrigin(UserScript::ValidUserScriptSchemes(),
                        url::Origin::Create(url).GetURL());
    PermissionSet tab_permissions(std::move(tab_api_permissions),
                                  ManifestPermissionSet(), tab_hosts.Clone(),
                                  tab_hosts.Clone());
    extension.permissions_data()->UpdateTabSpecificPermissions(kTabId,
                                                               tab_permissions);
  }

  void ClearActiveTab(const Extension& extension) {
    extension.permissions_data()->ClearTabSpecificPermissions(kTabId);
  }

  const Extension& all_urls() { return *all_urls_; }

  const Extension& active_tab() { return *active_tab_; }

  const Extension& page_capture() { return *page_capture_; }

  static constexpr int kTabId = 42;

 private:
  void SetUp() override {
    all_urls_ = ExtensionBuilder("all urls")
                    .AddPermission("<all_urls>")
                    .SetID(std::string(32, 'a'))
                    .Build();
    active_tab_ = ExtensionBuilder("active tab")
                      .AddPermission("activeTab")
                      .SetID(std::string(32, 'b'))
                      .Build();
    page_capture_ = ExtensionBuilder("page capture")
                        .AddPermission("pageCapture")
                        .AddPermission("activeTab")
                        .SetID(std::string(32, 'd'))
                        .Build();
  }

  void TearDown() override {
    all_urls_ = nullptr;
    active_tab_ = nullptr;
    page_capture_ = nullptr;
  }

  scoped_refptr<const Extension> all_urls_;
  scoped_refptr<const Extension> active_tab_;
  scoped_refptr<const Extension> page_capture_;
};

TEST_F(CaptureVisiblePageTest, URLsCapturableWithEitherActiveTabOrAllURLs) {
  const GURL test_urls[] = {
      // Normal web page.
      GURL("https://example.com"),

      // IPv6 pages should behave like normal web pages.
      GURL("http://[2607:f8b0:4005:805::200e]"),

      // filesystem: urls with web origins should behave like normal web pages.
      // TODO(https://crbug.com/853392): filesystem: URLs don't work with
      // activeTab.
      // GURL("filesystem:http://example.com/foo"),

      // blob: urls with web origins should behave like normal web pages.
      // TODO(https://crbug.com/853392): blob: URLs don't work with activeTab.
      // GURL("blob:http://example.com/bar"),
  };

  for (const GURL& url : test_urls) {
    SCOPED_TRACE(url.spec());
    EXPECT_TRUE(CanCapture(
        all_urls(), url, extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    GrantActiveTab(active_tab(), url);
    EXPECT_TRUE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    ClearActiveTab(active_tab());
    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_TRUE(CanCapture(page_capture(), url,
                           extensions::CaptureRequirement::kPageCapture));
    GrantActiveTab(page_capture(), url);
    EXPECT_TRUE(CanCapture(page_capture(), url,
                           extensions::CaptureRequirement::kPageCapture));
  }
}

TEST_F(CaptureVisiblePageTest, URLsCapturableOnlyWithActiveTab) {
  // The following URLs are origins that extensions are able to capture only if
  // they have activeTab granted. We require explicit user action for a higher
  // bar, since normally these pages are completely restricted to extensions at
  // all.
  const GURL test_urls[] = {
      // Another extension's URL.
      GURL("chrome-extension://cccccccccccccccccccccccccccccccc/foo.html"),

      // filesystem: urls behave like the underlying origin.
      // https://crbug.com/853392: filesystem: URLs don't work with activeTab.
      // GURL("filesystem:chrome-extension://cccccccccccccccccccccccccccccccc/foo"),

      // blob: urls behave like the underlying origin.
      // https://crbug.com/853392: blob: URLs don't work with activeTab.
      // GURL("blob:chrome-extension://cccccccccccccccccccccccccccccccc/bar"),

      // data: urls have no associated origin, so are more restricted.
      GURL("data:text/html;charset=utf-8,<html>Hello!</html>"),

      // A chrome:-scheme page.
      GURL(chrome::kChromeUISettingsURL),

      // The NTP.
      GURL(chrome::kChromeUINewTabURL),

      // The Chrome Web Store URL.
      ExtensionsClient::Get()->GetWebstoreBaseURL(),

      // The new Chrome Web Store URL.
      ExtensionsClient::Get()->GetNewWebstoreBaseURL(),
  };

  for (const GURL& url : test_urls) {
    SCOPED_TRACE(url.spec());
    EXPECT_FALSE(CanCapture(
        all_urls(), url, extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    GrantActiveTab(active_tab(), url);
    EXPECT_TRUE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    ClearActiveTab(active_tab());
    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_FALSE(CanCapture(page_capture(), url,
                            extensions::CaptureRequirement::kPageCapture));
    GrantActiveTab(page_capture(), url);
    EXPECT_TRUE(CanCapture(page_capture(), url,
                           extensions::CaptureRequirement::kPageCapture));
    ClearActiveTab(page_capture());
    EXPECT_FALSE(CanCapture(page_capture(), url,
                            extensions::CaptureRequirement::kPageCapture));
  }
}

// TODO(crbug.com/1041309): Add support for capturing chrome-untrusted://.
TEST_F(CaptureVisiblePageTest, ChromeUntrustedSchemeNotCaptured) {
  const GURL chrome_untrusted_url(kChromeUntrustedURL);

  SCOPED_TRACE(chrome_untrusted_url.spec());
  EXPECT_FALSE(CanCapture(all_urls(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kActiveTabOrAllUrls));

  EXPECT_FALSE(CanCapture(active_tab(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kActiveTabOrAllUrls));
  GrantActiveTab(active_tab(), chrome_untrusted_url);
  EXPECT_FALSE(CanCapture(active_tab(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kActiveTabOrAllUrls));
  ClearActiveTab(active_tab());
  EXPECT_FALSE(CanCapture(active_tab(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kActiveTabOrAllUrls));

  EXPECT_FALSE(CanCapture(page_capture(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kPageCapture));
  GrantActiveTab(page_capture(), chrome_untrusted_url);
  EXPECT_FALSE(CanCapture(page_capture(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kPageCapture));
  ClearActiveTab(page_capture());
  EXPECT_FALSE(CanCapture(page_capture(), chrome_untrusted_url,
                          extensions::CaptureRequirement::kPageCapture));
}

TEST_F(CaptureVisiblePageTest, SelfExtensionURLs) {
  auto get_filesystem_url_for_extension = [](const Extension& extension) {
    return GURL(base::StringPrintf("filesystem:chrome-extension://%s/foo",
                                   extension.id().c_str()));
  };
  auto get_blob_url_for_extension = [](const Extension& extension) {
    return GURL(base::StringPrintf("blob:chrome-extension://%s/foo",
                                   extension.id().c_str()));
  };

  // Extensions should be allowed to capture their own pages with either
  // activeTab or <all_urls>. We still require one of the two because there may
  // be other web content within the extension page, so we can't auto-grant
  // access.

  {
    EXPECT_TRUE(
        CanCapture(all_urls(), all_urls().GetResourceURL("foo.html"),
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    EXPECT_TRUE(
        CanCapture(all_urls(), get_filesystem_url_for_extension(all_urls()),
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    EXPECT_TRUE(
        CanCapture(all_urls(), get_blob_url_for_extension(all_urls()),
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    EXPECT_TRUE(CanCapture(page_capture(),
                           page_capture().GetResourceURL("foo.html"),
                           extensions::CaptureRequirement::kPageCapture));
    EXPECT_TRUE(CanCapture(page_capture(),
                           get_filesystem_url_for_extension(page_capture()),
                           extensions::CaptureRequirement::kPageCapture));
    EXPECT_TRUE(CanCapture(page_capture(),
                           get_blob_url_for_extension(page_capture()),
                           extensions::CaptureRequirement::kPageCapture));
  }

  const GURL active_tab_extension_urls[] = {
      active_tab().GetResourceURL("foo.html"),
      // https://crbug.com/853392: filesystem: URLs don't work with activeTab.
      // get_filesystem_url_for_extension(active_tab()),
      // https://crbug.com/853392: blob: URLs don't work with activeTab.
      // get_blob_url_for_extension(active_tab()),
  };

  for (const GURL& url : active_tab_extension_urls) {
    SCOPED_TRACE(url);

    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    GrantActiveTab(active_tab(), url);
    EXPECT_TRUE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    ClearActiveTab(active_tab());
    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
  }
  const GURL page_capture_extension_urls[] = {
      page_capture().GetResourceURL("foo.html"),
  };

  for (const GURL& url : page_capture_extension_urls) {
    SCOPED_TRACE(url);

    EXPECT_TRUE(CanCapture(page_capture(), url,
                           extensions::CaptureRequirement::kPageCapture));
  }
}

TEST_F(CaptureVisiblePageTest, NoPermissions) {
  const scoped_refptr<const Extension> no_permissions =
      ExtensionBuilder("no_page capture").Build();
  const GURL url("https://example.com");

  EXPECT_FALSE(CanCapture(*no_permissions, url,
                          extensions::CaptureRequirement::kPageCapture));
  EXPECT_FALSE(CanCapture(*no_permissions, url,
                          extensions::CaptureRequirement::kActiveTabOrAllUrls));
}

TEST_F(CaptureVisiblePageTest, PolicyBlockedURLs) {
  {
    URLPattern example_com(URLPattern::SCHEME_ALL, "https://example.com/*");
    URLPattern chrome_settings(URLPattern::SCHEME_ALL,
                               chrome::kChromeUISettingsURL);
    chrome_settings.SetPath("*");
    int context_id = 7;
    URLPatternSet blocked_patterns({example_com, chrome_settings});
    all_urls().permissions_data()->SetContextId(context_id);
    all_urls().permissions_data()->SetUsesDefaultHostRestrictions();
    PermissionsData::SetDefaultPolicyHostRestrictions(
        context_id, blocked_patterns, URLPatternSet());
    active_tab().permissions_data()->SetContextId(context_id);
    active_tab().permissions_data()->SetUsesDefaultHostRestrictions();
    page_capture().permissions_data()->SetContextId(context_id);
    page_capture().permissions_data()->SetUsesDefaultHostRestrictions();
  }

  const GURL test_urls[] = {
      GURL("https://example.com"), GURL(chrome::kChromeUISettingsURL),
  };

  for (const GURL& url : test_urls) {
    SCOPED_TRACE(url);
    EXPECT_FALSE(CanCapture(
        all_urls(), url, extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    GrantActiveTab(active_tab(), url);
    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));
    ClearActiveTab(active_tab());
    EXPECT_FALSE(
        CanCapture(active_tab(), url,
                   extensions::CaptureRequirement::kActiveTabOrAllUrls));

    EXPECT_FALSE(CanCapture(page_capture(), url,
                            extensions::CaptureRequirement::kPageCapture));
    GrantActiveTab(page_capture(), url);
    EXPECT_FALSE(CanCapture(page_capture(), url,
                            extensions::CaptureRequirement::kPageCapture));
    ClearActiveTab(page_capture());
    EXPECT_FALSE(CanCapture(page_capture(), url,
                            extensions::CaptureRequirement::kPageCapture));
  }
}

}  // namespace extensions
