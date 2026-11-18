// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using AccountMigrationWelcomeBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(AccountMigrationWelcomeBrowserTest, All) {
  set_test_loader_host(ash::kChromeUIAccountMigrationWelcomeHost);
  RunTest("chromeos/account_manager/account_migration_welcome_test.js",
          "mocha.run()");
}
