// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_test.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

class ShelfGuestSessionBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kGuestSession);
    command_line->AppendSwitch(::switches::kIncognito);
    command_line->AppendSwitchASCII(ash::switches::kLoginProfile, "hash");
    command_line->AppendSwitchASCII(
        ash::switches::kLoginUser,
        user_manager::GuestAccountId().GetUserEmail());
  }
};

// Tests that in guest session, shelf alignment could be initialized to bottom
// aligned, instead of bottom locked (crbug.com/699661).
IN_PROC_BROWSER_TEST_F(ShelfGuestSessionBrowserTest, ShelfAlignment) {
  // Check the alignment pref for the primary display.
  ash::ShelfAlignment alignment = ash::GetShelfAlignmentPref(
      browser()->profile()->GetPrefs(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::ShelfAlignment::kBottom, alignment);

  // Check the locked state, which is not exposed via prefs.
  EXPECT_FALSE(ash::ShelfTestApi().IsAlignmentBottomLocked());
}
