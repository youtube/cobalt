// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/common/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"

namespace {
constexpr char kGoodCrxId[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
}  // namespace

namespace extensions {

// Tests interaction between supervised users and extensions after the optional
// supervision is removed from the account.
class SupervisionRemovalExtensionTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  SupervisionRemovalExtensionTest() {
    if (AreExtensionsPermissionsEnabled()) {
      scoped_feature_list_.InitAndEnableFeature(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          supervised_user::
              kEnableExtensionsPermissionsForSupervisedUsersOnDesktop);
    }
  }

  ~SupervisionRemovalExtensionTest() override { scoped_feature_list_.Reset(); }

  // We have to essentially replicate what MixinBasedInProcessBrowserTest does
  // here because ExtensionBrowserTest doesn't inherit from that class.
  void SetUp() override {
    mixin_host_.SetUp();
    ExtensionBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpCommandLine(command_line);
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    mixin_host_.SetUpDefaultCommandLine(command_line);
    ExtensionBrowserTest::SetUpDefaultCommandLine(command_line);
  }

  bool SetUpUserDataDirectory() override {
    return mixin_host_.SetUpUserDataDirectory() &&
           ExtensionBrowserTest::SetUpUserDataDirectory();
  }

  void SetUpInProcessBrowserTestFixture() override {
    mixin_host_.SetUpInProcessBrowserTestFixture();
    ExtensionBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    mixin_host_.CreatedBrowserMainParts(browser_main_parts);
    ExtensionBrowserTest::CreatedBrowserMainParts(browser_main_parts);
  }

  void SetUpOnMainThread() override {
    mixin_host_.SetUpOnMainThread();
    ExtensionBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    mixin_host_.TearDownOnMainThread();
    ExtensionBrowserTest::TearDownOnMainThread();
  }

  void TearDownInProcessBrowserTestFixture() override {
    mixin_host_.TearDownInProcessBrowserTestFixture();
    ExtensionBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void TearDown() override {
    mixin_host_.TearDown();
    ExtensionBrowserTest::TearDown();
  }

 protected:
  bool IsDisabledForCustodianApproval(const std::string& extension_id) {
    ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
    return extension_prefs->HasDisableReason(
        extension_id,
        extensions::disable_reason::DISABLE_CUSTODIAN_APPROVAL_REQUIRED);
  }

  bool AreExtensionsPermissionsEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  InProcessBrowserTestMixinHost mixin_host_;

  // In order to simulate supervision removal and re-authentication use
  // supervised account in the PRE test and regular account afterwards.
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      {.sign_in_mode =
           content::IsPreTest()
               ? supervised_user::SupervisionMixin::SignInMode::kSupervised
               : supervised_user::SupervisionMixin::SignInMode::kRegular}};
};

// If extension permissions are enabled, removing supervision should also
// remove associated disable reasons, such as
// DISABLE_CUSTODIAN_APPROVAL_REQUIRED. Extensions should become enabled again
// after removing supervision. Prevents a regression to crbug/1045625.
// If extension permissions are disabled, removing supervision leaves the
// extension unchanged and enabled.
IN_PROC_BROWSER_TEST_P(SupervisionRemovalExtensionTest,
                       PRE_RemoveCustodianApprovalRequirement) {
  supervised_user_test_util::
      SetSupervisedUserExtensionsMayRequestPermissionsPref(profile(), true);

  ASSERT_TRUE(profile()->IsChild());

  base::FilePath path = test_data_dir_.AppendASCII("good.crx");
  bool extension_should_be_loaded = !AreExtensionsPermissionsEnabled();
  EXPECT_EQ(LoadExtension(path) != nullptr, extension_should_be_loaded);
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  if (AreExtensionsPermissionsEnabled()) {
    // This extension is a supervised user initiated install and should remain
    // disabled.
    EXPECT_TRUE(
        extension_registry()->disabled_extensions().Contains(kGoodCrxId));
    EXPECT_TRUE(IsDisabledForCustodianApproval(kGoodCrxId));
  } else {
    // When extension permissions are disabled, the extension is installed and
    // enabled.
    EXPECT_TRUE(
        extension_registry()->enabled_extensions().Contains(kGoodCrxId));
  }
}

IN_PROC_BROWSER_TEST_P(SupervisionRemovalExtensionTest,
                       RemoveCustodianApprovalRequirement) {
  ASSERT_FALSE(profile()->IsChild());

  // The extension should still be installed since we are sharing the same data
  // directory as the PRE test.
  const Extension* extension =
      extension_registry()->GetInstalledExtension(kGoodCrxId);
  EXPECT_TRUE(extension);

  // The extension should be enabled now after removing supervision.
  EXPECT_TRUE(extension_registry()->enabled_extensions().Contains(kGoodCrxId));
  EXPECT_FALSE(
      extension_registry()->disabled_extensions().Contains(kGoodCrxId));

  EXPECT_FALSE(IsDisabledForCustodianApproval(kGoodCrxId));
}

INSTANTIATE_TEST_SUITE_P(All, SupervisionRemovalExtensionTest, testing::Bool());

}  // namespace extensions
