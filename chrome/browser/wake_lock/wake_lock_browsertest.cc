// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/permissions/test/permission_request_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"

namespace {

// Handles HTTP requests to |path| with |content| as the response body.
// |content| is expected to be JavaScript; the response mime type is always set
// to "text/javascript".
// Invokes |done_callback| after serving the HTTP request.
std::unique_ptr<net::test_server::HttpResponse> RespondWithJS(
    const std::string& path,
    const std::string& content,
    base::OnceClosure done_callback,
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  if (request_url.path() != path)
    return nullptr;

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/javascript");
  response->set_content(content);
  std::move(done_callback).Run();
  return response;
}

}  // namespace

class WakeLockBrowserTest : public InProcessBrowserTest {
 protected:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Shorthand for starting the embedded web server and navigating to
  // simple.html.
  // Tests calling this usually call content::EvalJs() afterwards to run custom
  // code on the dummy page.
  void NavigateToSimplePage();

  // Registers a handle for "/js-response" in the embedded web server that
  // responds with |script| as the response body, and then navigates to |path|.
  // |path| usually points to a page that will somehow make a request to
  // "/js-response".
  void NavigateToAndRespondWithScript(const std::string& path,
                                      const std::string& script);
};

void WakeLockBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                  "SystemWakeLock");
}

void WakeLockBrowserTest::NavigateToSimplePage() {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/simple.html")));
}

void WakeLockBrowserTest::NavigateToAndRespondWithScript(
    const std::string& path,
    const std::string& script) {
  base::RunLoop loop;
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &RespondWithJS, "/js-response", script, loop.QuitClosure()));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(path)));
  loop.Run();
}

// https://w3c.github.io/screen-wake-lock/#the-request-method
// Screen locks are never allowed from workers.
IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestScreenLockFromWorker) {
  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  const std::string kWorkerScript =
      "navigator.wakeLock.request('screen').catch(err => "
      "    self.postMessage(err.name))";
  NavigateToAndRespondWithScript(
      "/workers/create_dedicated_worker.html?worker_url=/js-response",
      kWorkerScript);
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "waitForMessage();"));
  EXPECT_FALSE(observer.request_shown());
}

// Requests for a system lock should always be denied, and there should be no
// permission prompt.
IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestSystemLockFromWorker) {
  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  const std::string kWorkerScript =
      "navigator.wakeLock.request('system').catch(err => "
      "    self.postMessage(err.name))";
  NavigateToAndRespondWithScript(
      "/workers/create_dedicated_worker.html?worker_url=/js-response",
      kWorkerScript);
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "waitForMessage();"));
  EXPECT_FALSE(observer.request_shown());
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestPermissionScreen) {
  // Requests for a screen lock should always be granted, and there should be no
  // permission prompt.
  NavigateToSimplePage();

  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ("granted", content::EvalJs(
                           browser()->tab_strip_model()->GetActiveWebContents(),
                           "navigator.wakeLock.request('screen').then(lock => {"
                           "    lock.release(); return 'granted'; });"));
  EXPECT_FALSE(observer.request_shown());
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest,
                       RequestPermissionScreenNoUserGesture) {
  // Requests for a screen lock should always be granted, and there should be no
  // permission prompt.
  NavigateToSimplePage();

  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(
      "granted",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "navigator.wakeLock.request('screen').then(lock => {"
                      "    lock.release(); return 'granted'; });",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(observer.request_shown());
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest, RequestPermissionSystem) {
  // Requests for a system lock should always be denied, and there should be no
  // permission prompt.
  NavigateToSimplePage();

  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "navigator.wakeLock.request('system').catch(err => {"
                      "    return err.name; });"));
  EXPECT_FALSE(observer.request_shown());
}

IN_PROC_BROWSER_TEST_F(WakeLockBrowserTest,
                       RequestPermissionSystemNoUserGesture) {
  // Requests for a system lock should always be denied, and there should be no
  // permission prompt.
  NavigateToSimplePage();

  permissions::PermissionRequestObserver observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "navigator.wakeLock.request('system').catch(err => {"
                      "    return err.name; });",
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_FALSE(observer.request_shown());
}
