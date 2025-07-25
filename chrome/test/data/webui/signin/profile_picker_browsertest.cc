// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class ProfilePickerBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ProfilePickerBrowserTest() {
    set_test_loader_host(chrome::kChromeUIProfilePickerHost);
  }
};

using ProfilePickerTest = ProfilePickerBrowserTest;

IN_PROC_BROWSER_TEST_F(ProfilePickerTest, ProfileTypeChoice) {
  RunTest("signin/profile_type_choice_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProfilePickerTest, App) {
  RunTest("signin/profile_picker_app_test.js", "mocha.run()");
}

// TODO(crbug.com/1494777): Test is flaky.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_MainView DISABLED_MainView
#else
#define MAYBE_MainView MainView
#endif
IN_PROC_BROWSER_TEST_F(ProfilePickerTest, MAYBE_MainView) {
  RunTest("signin/profile_picker_main_view_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProfilePickerTest, ProfileCardMenu) {
  RunTest("signin/profile_card_menu_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(ProfilePickerTest, ProfileSwitch) {
  RunTest("signin/profile_switch_test.js", "mocha.run()");
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfilePickerTest, AccountSelectionLacros) {
  RunTest("signin/account_selection_lacros_test.js", "mocha.run()");
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
