// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/log_web_ui_url.h"

#include <stdint.h>

#include <vector>

#include "base/hash/hash.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_preload_manager_test_api.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::Bucket;
using testing::ElementsAre;

namespace webui {

class LogWebUIUrlTest : public InProcessBrowserTest {
 public:
  LogWebUIUrlTest() = default;

  LogWebUIUrlTest(const LogWebUIUrlTest&) = delete;
  LogWebUIUrlTest& operator=(const LogWebUIUrlTest&) = delete;

  ~LogWebUIUrlTest() override = default;

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  void RunTest(std::u16string title, const GURL& url) {
    EXPECT_THAT(histogram_tester_.GetAllSamples(webui::kWebUICreatedForUrl),
                ::testing::IsEmpty());

    auto* tab = browser()->tab_strip_model()->GetActiveWebContents();
    content::TitleWatcher title_watcher(tab, title);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    ASSERT_EQ(title, title_watcher.WaitAndGetTitle());
    uint32_t origin_hash = base::Hash(url.DeprecatedGetOriginAsURL().spec());
    EXPECT_THAT(histogram_tester_.GetAllSamples(webui::kWebUICreatedForUrl),
                ElementsAre(Bucket(origin_hash, 1)));
  }

 private:
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestExtensionsPage) {
  RunTest(
      l10n_util::GetStringUTF16(IDS_MANAGE_EXTENSIONS_SETTING_WINDOWS_TITLE),
      GURL(chrome::kChromeUIExtensionsURL));
}

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestHistoryPage) {
  RunTest(l10n_util::GetStringUTF16(IDS_HISTORY_TITLE),
          GURL(chrome::kChromeUIHistoryURL));
}

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestSettingsPage) {
  RunTest(l10n_util::GetStringUTF16(IDS_SETTINGS_SETTINGS),
          GURL(chrome::kChromeUISettingsURL));
}

IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestDinoPage) {
  GURL url = content::GetWebUIURL(content::kChromeUIDinoHost);
  // When a page does not have a dedicated title the URL with a trailing slash
  // is displayed as the title.
  RunTest(base::UTF8ToUTF16(url.GetWithEmptyPath().spec()), url);
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, TestChromeUntrustedPage) {
  RunTest(u"", GURL(base::StrCat(
                   {chrome::kChromeUIUntrustedPrintURL, "1/1/print.pdf"})));
}
#endif

// Tests that WebUI.ShownURL is logged after showing a WebUI.
IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, ShownWebUI) {
  const GURL url(chrome::kChromeUIHistoryURL);
  EXPECT_THAT(histogram_tester().GetAllSamples(webui::kWebUICreatedForUrl),
              ::testing::IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples(webui::kWebUIShownUrl),
              ::testing::IsEmpty());

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  ASSERT_TRUE(content::NavigateToURL(web_contents.get(), url));
  ASSERT_TRUE(
      content::WaitForRenderFrameReady(web_contents->GetPrimaryMainFrame()));

  uint32_t origin_hash = base::Hash(url.DeprecatedGetOriginAsURL().spec());
  EXPECT_THAT(histogram_tester().GetAllSamples(webui::kWebUICreatedForUrl),
              ElementsAre(Bucket(origin_hash, 1)));
  // The content is not visible before attaching it to the browser.
  EXPECT_THAT(
      histogram_tester().GetBucketCount(webui::kWebUIShownUrl, origin_hash), 1);
}

// Tests that WebUI.ShownURL is logged after showing a preloaded WebUI.
IN_PROC_BROWSER_TEST_F(LogWebUIUrlTest, ShownWebUIForPreloadedPage) {
  // Only top-chrome WebUIs are preloaded. Use Tab Search as an example.
  const GURL url(chrome::kChromeUITabSearchURL);
  uint32_t origin_hash = base::Hash(url.DeprecatedGetOriginAsURL().spec());
  EXPECT_THAT(histogram_tester().GetAllSamples(webui::kWebUICreatedForUrl),
              ::testing::IsEmpty());
  EXPECT_THAT(histogram_tester().GetAllSamples(webui::kWebUIShownUrl),
              ::testing::IsEmpty());

  // Preload the WebUI. The WebUI is created but not shown.
  WebUIContentsPreloadManagerTestAPI preload_test_api;
  preload_test_api.PreloadUrl(browser()->profile(), url);
  EXPECT_TRUE(
      content::WaitForLoadStop(preload_test_api.GetPreloadedWebContents()));
  EXPECT_THAT(histogram_tester().GetBucketCount(webui::kWebUICreatedForUrl,
                                                origin_hash),
              1);
  EXPECT_THAT(
      histogram_tester().GetBucketCount(webui::kWebUIShownUrl, origin_hash), 0);

  // Show the WebUI.
  std::unique_ptr<content::WebContents> web_contents =
      std::move(preload_test_api.preload_manager()
                    ->Request(url, browser()->profile())
                    .web_contents);
  EXPECT_THAT(
      histogram_tester().GetBucketCount(webui::kWebUIShownUrl, origin_hash), 1);
}

}  // namespace webui
