// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using InternetConfigDialogBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(InternetConfigDialogBrowserTest, All) {
  set_test_loader_host(ash::kChromeUIInternetConfigDialogHost);
  RunTest("chromeos/internet_config_dialog_test.js", "mocha.run()");
}
