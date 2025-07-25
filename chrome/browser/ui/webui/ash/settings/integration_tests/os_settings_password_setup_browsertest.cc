// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/test_support/os_settings_lock_screen_browser_test_base.h"
#include "chrome/test/data/webui/settings/chromeos/os_people_page/password_settings_api.test-mojom-test-utils.h"
#include "chrome/test/data/webui/settings/chromeos/test_api.test-mojom-test-utils.h"
#include "content/public/test/browser_test.h"

namespace ash::settings {

class OSSettingsPasswordSetupTest : public OSSettingsLockScreenBrowserTestBase {
  using OSSettingsLockScreenBrowserTestBase::
      OSSettingsLockScreenBrowserTestBase;

 public:
  mojom::PasswordSettingsApiAsyncWaiter GoToPasswordSettings(
      mojom::LockScreenSettingsAsyncWaiter& lock_screen_settings) {
    password_settings_remote_ =
        mojo::Remote(lock_screen_settings.GoToPasswordSettings());
    return mojom::PasswordSettingsApiAsyncWaiter(
        password_settings_remote_.get());
  }

 private:
  mojo::Remote<mojom::PasswordSettingsApi> password_settings_remote_;
};

class OSSettingsPasswordSetupTestWithGaiaPassword
    : public OSSettingsPasswordSetupTest {
 public:
  OSSettingsPasswordSetupTestWithGaiaPassword()
      : OSSettingsPasswordSetupTest(PasswordType::kGaia) {}
};
class OSSettingsPasswordSetupTestWithLocalPassword
    : public OSSettingsPasswordSetupTest {
 public:
  OSSettingsPasswordSetupTestWithLocalPassword()
      : OSSettingsPasswordSetupTest(PasswordType::kLocal) {}
};

IN_PROC_BROWSER_TEST_F(OSSettingsPasswordSetupTestWithGaiaPassword, NotShown) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  lock_screen_settings.AssertPasswordControlVisibility(false);
}

IN_PROC_BROWSER_TEST_F(OSSettingsPasswordSetupTestWithLocalPassword, Selected) {
  mojom::LockScreenSettingsAsyncWaiter lock_screen_settings =
      OpenLockScreenSettingsAndAuthenticate();
  mojom::PasswordSettingsApiAsyncWaiter password_settings =
      GoToPasswordSettings(lock_screen_settings);

  // TODO(b/290917006): Reenable this check. This check did not succeed in a
  // debug build, where the tests seems to have timed out.
  //
  // password_settings.AssertSelectedPasswordType(mojom::PasswordType::kLocal);
}

}  // namespace ash::settings
