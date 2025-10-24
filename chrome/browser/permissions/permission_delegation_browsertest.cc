// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/device/public/cpp/test/scoped_geolocation_overrider.h"

class PermissionDelegationBrowserTest : public InProcessBrowserTest {
 public:
  PermissionDelegationBrowserTest()
      : geolocation_overrider_(
            std::make_unique<device::ScopedGeolocationOverrider>(0, 0)) {}

  PermissionDelegationBrowserTest(const PermissionDelegationBrowserTest&) =
      delete;
  PermissionDelegationBrowserTest& operator=(
      const PermissionDelegationBrowserTest&) = delete;

  ~PermissionDelegationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    permissions::PermissionRequestManager* manager =
        permissions::PermissionRequestManager::FromWebContents(
            GetWebContents());
    mock_permission_prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(manager);

    https_embedded_test_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_embedded_test_server_->ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(https_embedded_test_server_.get());
    ASSERT_TRUE(https_embedded_test_server_->Start());
  }

  void TearDownOnMainThread() override {
    mock_permission_prompt_factory_.reset();
    https_embedded_test_server_.reset();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return mock_permission_prompt_factory_.get();
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::EmbeddedTestServer* https_embedded_test_server() {
    return https_embedded_test_server_.get();
  }

 private:
  std::unique_ptr<permissions::MockPermissionPromptFactory>
      mock_permission_prompt_factory_;
  std::unique_ptr<net::EmbeddedTestServer> https_embedded_test_server_;
  std::unique_ptr<device::ScopedGeolocationOverrider> geolocation_overrider_;
};

IN_PROC_BROWSER_TEST_F(PermissionDelegationBrowserTest, DelegatedToTwoFrames) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Main frame is on a.com, iframe 1 is on b.com and iframe 2 is on c.com.
  GURL main_frame_url =
      https_embedded_test_server()->GetURL("a.com", "/two_iframes_blank.html");
  GURL iframe_url_1 =
      https_embedded_test_server()->GetURL("b.com", "/simple.html");
  GURL iframe_url_2 =
      https_embedded_test_server()->GetURL("c.com", "/simple.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));
  content::RenderFrameHost* main_frame =
      GetWebContents()->GetPrimaryMainFrame();

  // Delegate permission to both frames.
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe1').allow = 'geolocation *';"));
  EXPECT_TRUE(content::ExecJs(
      main_frame,
      "document.getElementById('iframe2').allow = 'geolocation *';"));

  // Load the iframes.
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe1", iframe_url_1));
  EXPECT_TRUE(
      content::NavigateIframeToURL(GetWebContents(), "iframe2", iframe_url_2));

  content::RenderFrameHost* frame_1 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe1"));
  EXPECT_NE(nullptr, frame_1);
  content::RenderFrameHost* frame_2 = content::FrameMatchingPredicate(
      GetWebContents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, "iframe2"));
  EXPECT_NE(nullptr, frame_2);

  // Request permission from the first iframe.
  EXPECT_EQ(true, content::EvalJs(
                      frame_1,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));

  // A prompt should have been shown with the top level origin rather than the
  // iframe origin.
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_TRUE(prompt_factory()->RequestOriginSeen(
      main_frame_url.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_1.DeprecatedGetOriginAsURL()));
  EXPECT_FALSE(prompt_factory()->RequestOriginSeen(
      iframe_url_2.DeprecatedGetOriginAsURL()));

  // Request permission from the second iframe. Because it was granted to the
  // top level frame, it should also be granted to this iframe and there should
  // be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      frame_2,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());

  // Request permission from the top level frame. It should already be granted
  // to this iframe and there should be no prompt.
  EXPECT_EQ(true, content::EvalJs(
                      main_frame,
                      "new Promise(resolve => {"
                      "  navigator.geolocation.getCurrentPosition(function(){ "
                      "    resolve(true); });"
                      "});"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
}
