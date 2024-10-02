// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace errors = manifest_errors;
namespace keys = manifest_keys;

class PlatformAppsManifestTest : public ChromeManifestTest {
};

TEST_F(PlatformAppsManifestTest, PlatformApps) {
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("init_valid_platform_app.json");
  // Ensure this is treated as platform app, which causes it to have isolated
  // storage in the browser process. See also
  // ExtensionUtilUnittest.HasIsolatedStorage.
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension.get()));

  extension =
      LoadAndExpectSuccess("init_valid_platform_app_no_manifest_version.json");
  EXPECT_EQ(2, extension->manifest_version());

  extension = LoadAndExpectSuccess("incognito_valid_platform_app.json");
  EXPECT_FALSE(IncognitoInfo::IsSplitMode(extension.get()));

  Testcase error_testcases[] = {
      Testcase("init_invalid_platform_app_2.json",
               errors::kBackgroundRequiredForPlatformApps),
      Testcase("init_invalid_platform_app_3.json",
               ErrorUtils::FormatErrorMessage(
                   errors::kInvalidManifestVersionUnsupported, "either 2 or 3",
                   "apps")),
  };
  RunTestcases(error_testcases, std::size(error_testcases), EXPECT_TYPE_ERROR);

  Testcase warning_testcases[] = {
      Testcase(
          "init_invalid_platform_app_1.json",
          "'app.launch' is only allowed for legacy packaged apps and hosted "
          "apps, but this is a packaged app."),
      Testcase("init_invalid_platform_app_4.json",
               "'background' is only allowed for extensions, legacy packaged "
               "apps, hosted apps, login screen extensions, and chromeos "
               "system extensions, but this is a packaged app."),
      Testcase("init_invalid_platform_app_5.json",
               "'background' is only allowed for extensions, legacy packaged "
               "apps, hosted apps, login screen extensions, and chromeos "
               "system extensions, but this is a packaged app."),
      Testcase("incognito_invalid_platform_app.json",
               "'incognito' is only allowed for extensions and legacy packaged "
               "apps, "
               "but this is a packaged app."),
  };
  RunTestcases(warning_testcases, std::size(warning_testcases),
               EXPECT_TYPE_WARNING);
}

TEST_F(PlatformAppsManifestTest, PlatformAppContentSecurityPolicy) {
  // Normal platform apps can't specify a CSP value.
  Testcase warning_testcases[] = {
    Testcase(
        "init_platform_app_csp_warning_1.json",
        "'content_security_policy' is only allowed for extensions, legacy "
            "packaged apps, and login screen extensions, but this is a "
            "packaged app."),
    Testcase(
        "init_platform_app_csp_warning_2.json",
        "'app.content_security_policy' is not allowed for specified extension "
            "ID.")
  };
  RunTestcases(warning_testcases, std::size(warning_testcases),
               EXPECT_TYPE_WARNING);

  // Allowlisted ones can (this is the ID corresponding to the base 64 encoded
  // key in the init_platform_app_csp.json manifest.)
  SimpleFeature::ScopedThreadUnsafeAllowlistForTest allowlist(
      "ahplfneplbnjcflhdgkkjeiglkkfeelb");
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("init_platform_app_csp.json");
  EXPECT_EQ(0U, extension->install_warnings().size())
      << "Unexpected warning " << extension->install_warnings()[0].message;
  EXPECT_TRUE(extension->is_platform_app());
  EXPECT_EQ("default-src 'self' https://www.google.com;",
            CSPInfo::GetResourceContentSecurityPolicy(extension.get(),
                                                      std::string()));

  // But even allowlisted ones must specify a secure policy.
  LoadAndExpectWarning(
      "init_platform_app_csp_insecure.json",
      ErrorUtils::FormatErrorMessage(errors::kInvalidCSPInsecureValueIgnored,
                                     keys::kPlatformAppContentSecurityPolicy,
                                     "http://www.google.com", "default-src"));
}

TEST_F(PlatformAppsManifestTest, CertainApisRequirePlatformApps) {
  // Put APIs here that should be restricted to platform apps, but that haven't
  // yet graduated from experimental.
  static constexpr const char* kPlatformAppExperimentalApis[] = {
      "dns",
      "serial",
  };
  // TODO(miket): When the first platform-app API leaves experimental, write
  // similar code that tests without the experimental flag.

  // This manifest is a skeleton used to build more specific manifests for
  // testing. The requirements are that (1) it be a valid platform app, and (2)
  // it contain no permissions dictionary.
  std::string error;
  absl::optional<base::Value::Dict> platform_app_manifest =
      LoadManifest("init_valid_platform_app.json", &error);
  ASSERT_TRUE(platform_app_manifest);

  std::vector<ManifestData> manifests;
  // Create each manifest.
  for (const char* api_name : kPlatformAppExperimentalApis) {
    base::Value::List permissions;
    permissions.Append("experimental");
    permissions.Append(api_name);
    platform_app_manifest->Set("permissions", std::move(permissions));
    manifests.emplace_back(platform_app_manifest->Clone());
  }
  // First try to load without any flags. This should warn for every API.
  for (const auto& manifest : manifests) {
    LoadAndExpectWarning(
        manifest,
        "'experimental' requires the 'experimental-extension-apis' "
        "command line switch to be enabled.");
  }

  // Now try again with the experimental flag set.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  for (const auto& manifest : manifests) {
    LoadAndExpectSuccess(manifest);
  }
}

}  // namespace extensions
