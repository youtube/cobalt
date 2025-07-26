// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/history_clusters/core/features.h"
#include "components/search/ntp_features.h"
#include "content/public/test/browser_test.h"

class GlicWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  GlicWebUIBrowserTest() { set_test_loader_host(chrome::kChromeUIGlicHost); }

  void SetUp() override {
    features_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
    WebUIMochaBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList features_;
};

IN_PROC_BROWSER_TEST_F(GlicWebUIBrowserTest, UnitTestWebview) {
  RunTest("glic/unit_tests/webview_test.js", "mocha.run()");
}
