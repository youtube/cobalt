// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/lens/lens_features.h"
#include "content/public/test/browser_test.h"

class LensSidePanelWebUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  LensSidePanelWebUIBrowserTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(chrome::kChromeUILensSidePanelHost);
    scoped_feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay},
        {lens::features::kLensOverlayContextualSearchbox});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using LensSidePanelTest = LensSidePanelWebUIBrowserTest;
IN_PROC_BROWSER_TEST_F(LensSidePanelTest, SidePanelResultsFrame) {
  RunTest("lens/side_panel/results_frame_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensSidePanelTest, SearchboxBackButton) {
  RunTest("lens/side_panel/searchbox_back_button_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensSidePanelTest, ErrorPage) {
  RunTest("lens/side_panel/error_page_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(LensSidePanelTest, GhostLoaderState) {
  RunTest("lens/side_panel/ghost_loader_state_test.js", "mocha.run()");
}

using LensGhostLoaderTest = LensSidePanelWebUIBrowserTest;
IN_PROC_BROWSER_TEST_F(LensGhostLoaderTest, GhostLoaderState) {
  RunTest("lens/ghost_loader/ghost_loader_state_test.js", "mocha.run()");
}
