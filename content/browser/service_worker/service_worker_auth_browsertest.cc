// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/base/net_errors.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/ssl_server_config.h"
#include "net/test/embedded_test_server/connection_tracker.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace content {

const char kWorkerHttpBasicAuthPath[] = "/workers/http_basic_auth?intercept";

// Serves a Basic Auth challenge.
std::unique_ptr<net::test_server::HttpResponse> HandleHttpAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kWorkerHttpBasicAuthPath)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_UNAUTHORIZED);
  http_response->AddCustomHeader("WWW-Authenticate",
                                 "Basic realm=\"test realm\"");
  return http_response;
}

// Tests TLS + service workers. Inspired by
// content/browser/worker_host/worker_browsertest.cc.
class ServiceWorkerTlsTest : public ContentBrowserTest {
 public:
  ServiceWorkerTlsTest() = default;

  void SetUpOnMainThread() override {
    ShellContentBrowserClient::Get()->set_select_client_certificate_callback(
        base::BindOnce(&ServiceWorkerTlsTest::OnSelectClientCertificate,
                       base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  int select_certificate_count() const { return select_certificate_count_; }

  GURL GetTestURL(const std::string& test_case, const std::string& query) {
    std::string url_string = "/workers/" + test_case + "?" + query;
    return embedded_test_server()->GetURL(url_string);
  }

 private:
  base::OnceClosure OnSelectClientCertificate(
      content::WebContents* web_contents,
      net::SSLCertRequestInfo* cert_request_info,
      net::ClientCertIdentityList client_certs,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) {
    select_certificate_count_++;
    return base::OnceClosure();
  }

  int select_certificate_count_ = 0;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that TLS client auth prompts for a page controlled by a service
// worker, when the service worker calls fetch() for the main resource.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTlsTest, ClientAuthFetchMainResource) {
  // Start an HTTPS server which doesn't need client certs.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::NO_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());

  // Load a page that installs the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), https_server.GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  // Set the HTTPS server to require client certs.
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.ResetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);

  // Load a page that the SW intercepts with respondWith(fetch()). It should
  // prompt client certificate selection. (The navigation fails because
  // this test doesn't select a client certificate.)
  EXPECT_FALSE(NavigateToURL(
      shell(), https_server.GetURL("/workers/simple.html?intercept")));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests that TLS client auth prompts for a page controlled by a service
// worker drop silently without crash if the client gets destroyed right
// before the UA receives the certification request. Regression test for
// https://crbug.com/1375795.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTlsTest,
                       ClientAuthFetchMainResourceAfterDestruction) {
  // 1. Start an HTTPS server which doesn't need client certs and initialize
  // trackers.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  net::test_server::ConnectionTracker connection_tracker{&https_server};
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::NO_CLIENT_CERT;
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);
  ASSERT_TRUE(https_server.Start());
  GURL service_worker_page_url =
      https_server.GetURL("/workers/service_worker_setup.html");
  GURL destruction_url = https_server.GetURL("/workers/simple.html?intercept");

  // 2. Open a new shell and load a page that installs the service worker.
  Shell* new_shell = Shell::CreateNewWindow(
      shell()->web_contents()->GetController().GetBrowserContext(),
      GURL::EmptyGURL(), nullptr, gfx::Size());
  EXPECT_TRUE(NavigateToURL(new_shell, service_worker_page_url));
  ASSERT_EQ("ok", EvalJs(new_shell, "setup();"));

  // 3. Set the HTTPS server to require client certs.
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.ResetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);

  // 4. Load a page that the SW intercepts with respondWith(fetch()), and then
  // hold the test until the client is attempting to build the connection with
  // the server. The server will ask the client for the certification soon.
  TestNavigationManager navigation_manager{new_shell->web_contents(),
                                           destruction_url};
  ASSERT_EQ(connection_tracker.GetAcceptedSocketCount(), 2U);
  std::ignore = ExecJs(new_shell, JsReplace("location = $1", destruction_url),
                       EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES);
  EXPECT_TRUE(navigation_manager.WaitForRequestStart());
  navigation_manager.ResumeNavigation();
  connection_tracker.WaitForAcceptedConnections(3);

  // 5. Close the tab.
  new_shell->Close();

  // Crash should not happen.
  EXPECT_FALSE(NavigateToURL(shell(), https_server.GetURL("/title1.html")));
}

// Tests that TLS client auth prompts for a page controlled by a service
// worker, when the service worker calls fetch() for a subresource.
IN_PROC_BROWSER_TEST_F(ServiceWorkerTlsTest, ClientAuthFetchSubResource) {
  // Load a page that installs the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), GetTestURL("service_worker_setup.html", std::string())));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  // Load a page controlled by the service worker.
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetTestURL("simple.html?fallback", std::string())));

  // Start HTTPS server.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  net::SSLServerConfig ssl_config;
  ASSERT_TRUE(https_server.Start());
  ssl_config.client_cert_type =
      net::SSLServerConfig::ClientCertType::REQUIRE_CLIENT_CERT;
  https_server.ResetSSLConfig(net::EmbeddedTestServer::CERT_OK, ssl_config);

  // Perform a fetch from the controlled page to the https server. It should
  // prompt client certificate selection. (The fetch fails because this test
  // doesn't select a client certificate.)
  std::string url = https_server.GetURL("/?intercept").spec();
  EXPECT_EQ("TypeError", EvalJs(shell(), "try_fetch('" + url + "');"));
  EXPECT_EQ(1, select_certificate_count());
}

// Tests basic authentication + service workers. Inspired by
// content/browser/browsing_data/browsing_data_remover_impl_browsertest.cc.
class ServiceWorkerBasicAuthTest : public ContentBrowserTest {
 public:
  ServiceWorkerBasicAuthTest()
      : ssl_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleHttpAuthRequest));
    EXPECT_TRUE(ssl_server_.Start());
  }

  void SetUpOnMainThread() override {
    login_requested_ = LoginRequested::kNone;
    // Set a login request callback to be used instead of a login dialog since
    // such a dialog is difficult to control programmatically and doesn't work
    // on all platforms.
    ShellContentBrowserClient::Get()->set_login_request_callback(
        base::BindLambdaForTesting([&](bool is_primary_main_frame) {
          login_requested_ = is_primary_main_frame ? LoginRequested::kMainFrame
                                                   : LoginRequested::kSubFrame;
        }));
  }

 protected:
  enum class LoginRequested { kNone, kMainFrame, kSubFrame };

  LoginRequested login_requested_ = LoginRequested::kNone;
  net::test_server::EmbeddedTestServer ssl_server_;
};

// Tests that basic auth prompts for a page controlled by a service
// worker, when the service worker calls fetch() for the main resource.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBasicAuthTest,
                       BasicAuthPromptFetchMainResourceMainFrame) {
  // Load a page that installs the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server_.GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  EXPECT_EQ(LoginRequested::kNone, login_requested_);
  // Because our login request callback does nothing, navigation should
  // fail.
  EXPECT_FALSE(
      NavigateToURL(shell(), ssl_server_.GetURL(kWorkerHttpBasicAuthPath)));
  EXPECT_EQ(LoginRequested::kMainFrame, login_requested_);
}

// Tests that basic auth prompts for a page controlled by a service
// worker, when the service worker calls fetch() for the main resource for
// subframe.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBasicAuthTest,
                       BasicAuthPromptFetchMainResourceSubframe) {
  // Load a page that installs the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server_.GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  EXPECT_EQ(LoginRequested::kNone, login_requested_);
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server_.GetURL("/workers/iframe_basic_auth.html")));
  // Login request callback should be called for a iframe's main resource.
  EXPECT_EQ(LoginRequested::kSubFrame, login_requested_);
}

// Tests that basic auth prompts for a page controlled by a service
// worker, when the service worker calls fetch() for a subresource.
IN_PROC_BROWSER_TEST_F(ServiceWorkerBasicAuthTest,
                       BasicAuthPromptFetchSubResource) {
  // Load a page that installs the service worker.
  EXPECT_TRUE(NavigateToURL(
      shell(), ssl_server_.GetURL("/workers/service_worker_setup.html")));
  EXPECT_EQ("ok", EvalJs(shell(), "setup();"));

  // Load a page controlled by the service worker.
  EXPECT_TRUE(
      NavigateToURL(shell(), ssl_server_.GetURL("/workers/simple.html")));

  EXPECT_EQ(LoginRequested::kNone, login_requested_);
  // Perform a fetch from the controlled page to the page which needs basic
  // auth (The fetch should return status code 401.)
  std::string url = ssl_server_.GetURL(kWorkerHttpBasicAuthPath).spec();
  EXPECT_EQ(401, EvalJs(shell(), "try_fetch_status('" + url + "');"));
  EXPECT_EQ(LoginRequested::kMainFrame, login_requested_);
}

}  // namespace content
