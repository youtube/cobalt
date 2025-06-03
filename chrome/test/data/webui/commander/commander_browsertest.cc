// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

using CommanderWebUIBrowserTest = WebUIMochaBrowserTest;

IN_PROC_BROWSER_TEST_F(CommanderWebUIBrowserTest, All) {
  set_test_loader_host(chrome::kChromeUICommanderHost);
  RunTest("commander/commander_app_test.js", "mocha.run()");
}
