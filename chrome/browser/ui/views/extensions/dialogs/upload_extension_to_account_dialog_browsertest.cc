// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/extensions_dialogs.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/mojom/manifest.mojom.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace extensions {

namespace {

// Signs a user in to the provided `profile`. This also provides a profile icon.
void SignIn(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      identity_manager, "testymctestface@gmail.com",
      signin::ConsentLevel::kSignin);
  signin::SimulateAccountImageFetch(identity_manager, account_info.account_id,
                                    "https://avatar.com/avatar.png",
                                    gfx::test::CreateImage(/*size=*/32));
  // Pretend the user has now explcitly signed in. All this is required for
  // extensions to sync in transport mode.
  profile->GetPrefs()->SetBoolean(prefs::kExplicitBrowserSignin, true);
}

}  // namespace

class UploadExtensionToAccountDialogBrowserTest
    : public ExtensionsDialogBrowserTest {
 public:
  UploadExtensionToAccountDialogBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        syncer::kSyncEnableExtensionsInTransportMode);
  }
  ~UploadExtensionToAccountDialogBrowserTest() override = default;
  UploadExtensionToAccountDialogBrowserTest(
      const UploadExtensionToAccountDialogBrowserTest&) = delete;
  UploadExtensionToAccountDialogBrowserTest& operator=(
      const UploadExtensionToAccountDialogBrowserTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    // Add an extension that can be uploaded. This means that it would be
    // syncable and is a local extension (i.e. not installed while a user is
    // signed in, therefore not associated with any signed in user).
    scoped_refptr<const Extension> extension;

    std::string extension_name;
    if (name == "NormalExtension") {
      extension_name = "My Extension";
    } else if (name == "LongNameExtension") {
      extension_name =
          "This extension has a really long name that should probably be "
          "truncated like seriously who thought this was a good idea?";
    }

    extension = ExtensionBuilder(extension_name)
                    .SetLocation(mojom::ManifestLocation::kInternal)
                    .Build();

    ASSERT_TRUE(extension);
    ExtensionSystem::Get(browser()->profile())
        ->extension_service()
        ->AddExtension(extension.get());

    // Sign in AFTER the extension has been added so there is an active account
    // for extensions to be uploaded to.
    SignIn(browser()->profile());

    ShowUploadExtensionToAccountDialog(browser(), *extension,
                                       /*accept_callback=*/base::DoNothing(),
                                       /*cancel_callback=*/base::DoNothing());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(UploadExtensionToAccountDialogBrowserTest,
                       InvokeUi_NormalExtension) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(UploadExtensionToAccountDialogBrowserTest,
                       InvokeUi_LongNameExtension) {
  ShowAndVerifyUi();
}

}  // namespace extensions
