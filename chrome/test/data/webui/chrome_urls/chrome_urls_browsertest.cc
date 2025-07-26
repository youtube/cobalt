// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/webui/chrome_urls/features.h"
#include "content/public/test/browser_test.h"

class ChromeURLsUiBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ChromeURLsUiBrowserTest() {
    set_test_loader_host(chrome::kChromeUIChromeURLsHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      chrome_urls::kInternalOnlyUisPref};
};

IN_PROC_BROWSER_TEST_F(ChromeURLsUiBrowserTest, App) {
  RunTest("chrome_urls/app_test.js", "mocha.run()");
}
