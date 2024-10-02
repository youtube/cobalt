// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "components/autofill/core/browser/logging/log_router.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

class PasswordManagerInternalsWebUIBrowserTest : public WebUIBrowserTest {
 public:
  PasswordManagerInternalsWebUIBrowserTest();
  ~PasswordManagerInternalsWebUIBrowserTest() override;

  void SetUpOnMainThread() override;

 protected:
  content::WebContents* GetWebContents();

  // Navigates to the internals page in a tab specified by |disposition| (and
  // optionally, by |browser|). Also assigns the corresponding UI controller to
  // |controller_|.
  void OpenInternalsPage(WindowOpenDisposition disposition);
  void OpenInternalsPageWithBrowser(Browser* browser,
                                    WindowOpenDisposition disposition);

 private:
  raw_ptr<PasswordManagerInternalsUI, DanglingUntriaged> controller_ = nullptr;
};

PasswordManagerInternalsWebUIBrowserTest::
    PasswordManagerInternalsWebUIBrowserTest() = default;

PasswordManagerInternalsWebUIBrowserTest::
    ~PasswordManagerInternalsWebUIBrowserTest() = default;

void PasswordManagerInternalsWebUIBrowserTest::SetUpOnMainThread() {
  WebUIBrowserTest::SetUpOnMainThread();
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);
  AddLibrary(base::FilePath(
      FILE_PATH_LITERAL("password_manager_internals_browsertest.js")));
}

content::WebContents*
PasswordManagerInternalsWebUIBrowserTest::GetWebContents() {
  return browser()->tab_strip_model()->GetActiveWebContents();
}

void PasswordManagerInternalsWebUIBrowserTest::OpenInternalsPage(
    WindowOpenDisposition disposition) {
  OpenInternalsPageWithBrowser(browser(), disposition);
}

void PasswordManagerInternalsWebUIBrowserTest::OpenInternalsPageWithBrowser(
    Browser* browser,
    WindowOpenDisposition disposition) {
  std::string url_string("chrome://");
  url_string += chrome::kChromeUIPasswordManagerInternalsHost;
  ui_test_utils::NavigateToURLWithDisposition(
      browser, GURL(url_string), disposition,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  controller_ = static_cast<PasswordManagerInternalsUI*>(
      GetWebContents()->GetWebUI()->GetController());
}

IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest, LogEntry) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  ASSERT_TRUE(RunJavascriptTest("testLogText"));
}

// Test that a single internals page is empty on load.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_EmptyOnLoad) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  ASSERT_TRUE(RunJavascriptTest("testLogEmpty"));
}

// Test that a single internals page is flushed on reload.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_FlushedOnReload) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  OpenInternalsPage(WindowOpenDisposition::CURRENT_TAB);  // Reload.
  ASSERT_TRUE(RunJavascriptTest("testLogEmpty"));
}

// Test that navigation away from the internals page works OK.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       LogEntry_NavigateAway) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  log_router->ProcessLog("<script> text for testing");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL(chrome::kChromeUIVersionURL)));
}

// Test that the description is correct in a non-Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       NonIncognitoMessage) {
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          browser()->profile());
  ASSERT_TRUE(log_router);
  ASSERT_TRUE(RunJavascriptTest("testNonIncognitoDescription"));
}

// Test that the description is correct in an Incognito tab.
IN_PROC_BROWSER_TEST_F(PasswordManagerInternalsWebUIBrowserTest,
                       IncognitoMessage) {
  Browser* incognito = CreateIncognitoBrowser();
  EXPECT_TRUE(incognito->profile()->IsOffTheRecord());
  autofill::LogRouter* log_router =
      password_manager::PasswordManagerLogRouterFactory::GetForBrowserContext(
          incognito->profile());
  EXPECT_FALSE(log_router);  // There should be no log_router for Incognito.
  OpenInternalsPageWithBrowser(incognito, WindowOpenDisposition::CURRENT_TAB);
  SetWebUIInstance(
      incognito->tab_strip_model()->GetActiveWebContents()->GetWebUI());
  ASSERT_TRUE(RunJavascriptTest("testIncognitoDescription"));
}
