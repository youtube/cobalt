// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/keep_alive_url_loader_service.h"

#include <memory>
#include <tuple>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/allow_check_is_test_for_testing.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/back_forward_cache_test_util.h"
#include "content/browser/loader/keep_alive_url_loader.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/keep_alive_url_loader_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/features.h"
#include "url/url_util.h"

namespace content {

namespace {

constexpr char16_t kPromiseResolvedPageTitle[] = u"Resolved";

constexpr char kPrimaryHost[] = "a.test";
constexpr char kSecondaryHost[] = "b.test";

constexpr char kKeepAliveEndpoint[] = "/beacon";

constexpr char k200TextResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "\r\n"
    "Acked!";

constexpr char kBeaconId[] = "beacon01";

constexpr char kFetchLaterEndpoint[] = "/fetch-later";

// Encodes the given `url` using the JS method encodeURIComponent.
std::string EncodeURL(const GURL& url) {
  url::RawCanonOutputT<char> buffer;
  url::EncodeURIComponent(url.spec(), &buffer);
  return std::string(buffer.view());
}

MATCHER(IsFrameHidden,
        base::StrCat({"Frame is", negation ? " not" : "", " hidden"})) {
  return arg->GetVisibilityState() == PageVisibilityState::kHidden;
}

}  // namespace

class KeepAliveURLBrowserTestBase : public ContentBrowserTest {
 protected:
  using FeaturesType = std::vector<base::test::FeatureRefAndParams>;
  using DisabledFeaturesType = std::vector<base::test::FeatureRef>;

  KeepAliveURLBrowserTestBase()
      : https_test_server_(std::make_unique<net::EmbeddedTestServer>(
            net::EmbeddedTestServer::TYPE_HTTPS)) {}
  ~KeepAliveURLBrowserTestBase() override = default;
  // Not Copyable.
  KeepAliveURLBrowserTestBase(const KeepAliveURLBrowserTestBase&) = delete;
  KeepAliveURLBrowserTestBase& operator=(const KeepAliveURLBrowserTestBase&) =
      delete;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                                GetDisabledFeatures());
    base::test::AllowCheckIsTestForTesting();
    ContentBrowserTest::SetUp();
  }
  virtual const FeaturesType& GetEnabledFeatures() {
    static const FeaturesType enabled_features =
        GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            {{blink::features::kKeepAliveInBrowserMigration, {}}});
    return enabled_features;
  }
  virtual const DisabledFeaturesType& GetDisabledFeatures() {
    static const DisabledFeaturesType disabled_features =
        GetDefaultDisabledBackForwardCacheFeaturesForTesting();
    return disabled_features;
  }

  void SetUpOnMainThread() override {
    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");
    loaders_observer_ = std::make_unique<KeepAliveURLLoadersTestObserver>(
        web_contents()->GetBrowserContext());

    // Initialize an HTTPS server. Subclass may choose to use HTTPS by calling
    // `SetUseHttps()`.
    https_test_server_->AddDefaultHandlers(GetTestDataFilePath());
    https_test_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);

    ContentBrowserTest::SetUpOnMainThread();
  }

 protected:
  [[nodiscard]] std::vector<
      std::unique_ptr<net::test_server::ControllableHttpResponse>>
  RegisterRequestHandlers(const std::vector<std::string>& relative_urls) {
    std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
        handlers;
    for (const auto& relative_url : relative_urls) {
      handlers.emplace_back(
          std::make_unique<net::test_server::ControllableHttpResponse>(
              server(), relative_url));
    }
    return handlers;
  }

  // Returns a cross-origin (kSecondaryHost) URL that causes the following
  // redirect chain:
  //     http://b.com:<port>/no-cors-server-redirect-307?...
  // --> http://b.com:<port>/server-redirect-307?...
  // --> http://b.com:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  GURL GetCrossOriginMultipleRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url2 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           target_url.spec().c_str()));
    const auto intermediate_url1 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/server-redirect-307?%s",
                                           intermediate_url2.spec().c_str()));
    return server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           intermediate_url1.spec().c_str()));
  }

  // Returns a same-origin (kPrimaryHost) URL that causes the following
  // redirect chain:
  //     /server-redirect-307?...
  // --> /no-cors-server-redirect-307?...
  // --> `target_url
  GURL GetSameOriginMultipleRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url1 = server()->GetURL(
        kPrimaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         target_url.spec().c_str()));
    return server()->GetURL(
        kPrimaryHost, base::StringPrintf("/server-redirect-307?%s",
                                         intermediate_url1.spec().c_str()));
  }

  // Returns a same-origin (kPrimaryHost) URL that leads to cross-origin
  // redirect chain:
  //     /server-redirect-307?...
  // --> http://b.com:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  GURL GetSameAndCrossOriginRedirectsURL(const GURL& target_url) const {
    const auto intermediate_url1 = server()->GetURL(
        kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                           target_url.spec().c_str()));
    return server()->GetURL(
        kPrimaryHost, base::StringPrintf("/server-redirect-307?%s",
                                         intermediate_url1.spec().c_str()));
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }
  RenderFrameHostImpl* current_frame_host() {
    return web_contents()->GetPrimaryFrameTree().root()->current_frame_host();
  }
  KeepAliveURLLoaderService* loader_service() {
    return static_cast<StoragePartitionImpl*>(
               web_contents()
                   ->GetBrowserContext()
                   ->GetDefaultStoragePartition())
        ->GetKeepAliveURLLoaderService();
  }
  void DisableBackForwardCache(WebContents* web_contents) {
    DisableBackForwardCacheForTesting(
        web_contents, BackForwardCache::TEST_REQUIRES_NO_CACHING);
  }
  KeepAliveURLLoadersTestObserver& loaders_observer() {
    return *loaders_observer_;
  }
  void SetUseHttps() { use_https_ = true; }
  net::EmbeddedTestServer* server() {
    return use_https_ ? https_test_server_.get() : embedded_test_server();
  }
  const net::EmbeddedTestServer* server() const {
    return use_https_ ? https_test_server_.get() : embedded_test_server();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<KeepAliveURLLoadersTestObserver> loaders_observer_;
  bool use_https_ = false;
  const std::unique_ptr<net::EmbeddedTestServer> https_test_server_;
};

// Contains the integration tests for loading fetch(url, {keepalive: true})
// requests via browser process that are difficult to reliably reproduce in web
// tests.
//
// Note that due to using different approach, tests to cover implementation
// before `kKeepAliveInBrowserMigration`, i.e. loading via delaying renderer
// shutdown, cannot be verified with inspecting KeepAliveURLLoaderService here
// and still live in a different file
// content/browser/renderer_host/render_process_host_browsertest.cc
class KeepAliveURLBrowserTest
    : public KeepAliveURLBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  // Navigates to a page specified by `keepalive_page_url`, which must fire a
  // fetch keepalive request.
  // The method then postpones the request handling until RFH of the page is
  // fully unloaded (by navigating to another cross-origin page).
  // After that, `response` will be sent back.
  // `keepalive_request_handler` must handle the fetch keepalive request.
  void LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      const GURL& keepalive_page_url,
      net::test_server::ControllableHttpResponse* keepalive_request_handler,
      const std::string& response) {
    ASSERT_TRUE(NavigateToURL(web_contents(), keepalive_page_url));
    RenderFrameHostImplWrapper rfh_1(current_frame_host());
    // Ensure the current page can be unloaded instead of being cached.
    DisableBackForwardCache(web_contents());
    // Ensure the keepalive request is sent before leaving the current page.
    keepalive_request_handler->WaitForRequest();
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // Navigate to cross-origin page to ensure the 1st page can be unloaded.
    ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
    // Ensure the 1st page has been unloaded.
    ASSERT_TRUE(rfh_1.WaitUntilRenderFrameDeleted());
    // The disconnected loader is still pending to receive response.
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
    ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);

    // Send back response to terminate in-browser request handling for the
    // pending request from 1st page.
    keepalive_request_handler->Send(response);
    keepalive_request_handler->Done();
  }

  GURL GetKeepAlivePageURL(const std::string& method,
                           size_t num_requests = 1,
                           bool set_csp = false) const {
    return server()->GetURL(
        kPrimaryHost,
        base::StringPrintf(
            "/set-header-with-file/content/test/data/fetch-keepalive.html?"
            "method=%s&requests=%zu%s",
            method.c_str(), num_requests,
            set_csp
                ? "&Content-Security-Policy: connect-src 'self' http://csp.test"
                : ""));
  }
  GURL GetCrossOriginPageURL() {
    return server()->GetURL(kSecondaryHost, "/title2.html");
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    KeepAliveURLBrowserTest,
    ::testing::Values(net::HttpRequestHeaders::kGetMethod,
                      net::HttpRequestHeaders::kPostMethod),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, OneRequest) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepAlivePageURL(method)));
  // Ensure the keepalive request is sent, but delay response.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // End the keepalive request by sending back response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Verify keepalive request loading works given 2 concurrent requests to the
// same host.
//
// Note: Chromium allows at most 6 concurrent connections to the same host under
// HTTP 1.1 protocol, which `server()` uses by default.
// Exceeding this limit will hang the browser.
// TODO(crbug.com/1428502): Flaky on Fuchsia and Android.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       DISABLED_TwoConcurrentRequestsPerHost) {
  const std::string method = GetParam();
  const size_t num_requests = 2;
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, kKeepAliveEndpoint});
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(
      NavigateToURL(web_contents(), GetKeepAlivePageURL(method, num_requests)));
  // Ensure all keepalive requests are sent, but delay responses.
  request_handlers[0]->WaitForRequest();
  request_handlers[1]->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), num_requests);

  // End the keepalive request by sending back responses.
  request_handlers[0]->Send(k200TextResponse);
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[0]->Done();
  request_handlers[1]->Done();

  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(2);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK, net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping has been unloaded. The browser must ensure the response is received and
// processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseAfterPageUnload) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(method), request_handler.get(), k200TextResponse);

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  // `KeepAliveURLLoader::OnComplete` may not be called, as renderer is dead.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Delays response to a keepalive ping until after the page making the keepalive
// ping is put into BackForwardCache. The response should be processed by the
// renderer after the page is restored from BackForwardCache.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveResponseInBackForwardCache) {
  const std::string method = GetParam();
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  ASSERT_TRUE(NavigateToURL(web_contents(), GetKeepAlivePageURL(method)));
  RenderFrameHostImplWrapper rfh_1(current_frame_host());
  // Ensure the keepalive request is sent before leaving the current page.
  request_handler->WaitForRequest();
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Navigate to cross-origin page.
  ASSERT_TRUE(NavigateToURL(web_contents(), GetCrossOriginPageURL()));
  // Ensure the previous page has been put into BackForwardCache.
  ASSERT_EQ(rfh_1->GetLifecycleState(),
            RenderFrameHost::LifecycleState::kInBackForwardCache);
  // The loader is still pending to receive response.
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

  // Send back response.
  request_handler->Send(k200TextResponse);
  // The response is immediately forwarded to the in-BackForwardCache renderer.
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  // Go back to `rfh_1`.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The response should be processed in renderer. Hence resolving Promise.
  TitleWatcher watcher(web_contents(), kPromiseResolvedPageTitle);
  EXPECT_EQ(watcher.WaitAndGetTitle(), kPromiseResolvedPageTitle);
  request_handler->Done();
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 4 URLs.
//
// As the mode is set to "no-cors" for CORS-safelisted requests, the redirect is
// processed without an error while the request is cross-origin.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest, MultipleRedirectsRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects and eventually points to a
  // cross-origin `target_url`:
  //
  //     http://b.com:<port>/no-cors-server-redirect-307?...
  // --> http://b.com:<port>/server-redirect-307?...
  // --> http://b.com:<port>/no-cors-server-redirect-307?...
  // --> `target_url
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));

  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'no-cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(3);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // Ensure the fetch request is sent.
  request_handler->WaitForRequest();
  // Send back response to terminate in-browser request handling.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  // All redirects and the response should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(3);
  loaders_observer().WaitForTotalOnReceiveResponseForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::OK});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 3 URLs, where the cross-origin URLs
// are the 2nd URL & the 3rd URL in the chain.
//
// As the mode is set to "cors" for CORS-safelisted requests, the redirect will
// fail at the first cross-origin URL.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MultipleRedirectsAndFailInBetweenRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  ASSERT_TRUE(server()->Start());

  // Set up a same-origin URL with CORS-safelisted payload that causes multiple
  // redirects and eventually points to a cross-origin `target_url`:
  //
  //     http://a.com:<port>/server-redirect-307?...
  // --> http://b.com:<port>/no-cors-server-redirect-307?... => should fail
  // --> `target_url => should not reach here
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetSameAndCrossOriginRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // No request will be sent to kKeepAliveEndpoint, as it fails at the 2nd URL.

  // All redirects should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(1);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::ERR_FAILED});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Tests fetch(..., {keepalive: true}) with a cross-origin & CORS-safelisted
// request that causes a redirect chain of 3 URLs, where the cross-origin URL
// is the target URL (3rd URL in the chain).
//
// As the mode is set to "cors" for CORS-safelisted requests, the redirect will
// fail at the first cross-origin URL.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       MultipleRedirectsAndFailAtLastRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a same-origin URL with CORS-safelisted payload that causes multiple
  // redirects and eventually points to a cross-origin `target_url`:
  //
  //     http://a.com:<port>/server-redirect-307?...
  // --> http://a.com:<port>/no-cors-server-redirect-307?...
  // --> `target_url => should fail to get response
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetSameOriginMultipleRedirectsURL(target_url);

  // Navigate to a page that calls fetch() API and verify its response.
  ASSERT_TRUE(NavigateToURL(web_contents(),
                            server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(),
                     JsReplace(R"(
    fetch($1, {keepalive: true, mode: 'cors'});
  )",
                               beacon_url),
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // The in-browser logic should handle all redirects in browser first.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(2);
  // After in-browser processing, the loader should remain alive to support
  // forwarding stored redirects/response to renderer.
  ASSERT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

  // No request will be sent to kKeepAliveEndpoint, as it fails at the 2nd URL.
  // The redirect request should be processed in browser and gets sent.
  request_handler->WaitForRequest();
  // End the keepalive request by sending back final response.
  request_handler->Send(k200TextResponse);
  request_handler->Done();

  // All redirects should be forwarded to renderer.
  loaders_observer().WaitForTotalOnReceiveRedirectForwarded(2);
  loaders_observer().WaitForTotalOnCompleteForwarded({net::ERR_FAILED});
  // After forwarding, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Delays handling redirect for a keepalive ping until after the page making the
// keepalive ping has been unloaded. The browser must ensure the redirect is
// verified and properly processed by the browser.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char redirect_target[] = "/beacon-redirected";
  auto request_handlers =
      RegisterRequestHandlers({kKeepAliveEndpoint, redirect_target});
  ASSERT_TRUE(server()->Start());

  // Sets up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://a.com:<port>/beacon-redirected
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(method), request_handlers[0].get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         redirect_target));

  // The in-browser logic should process the redirect.
  loaders_observer().WaitForTotalOnReceiveRedirectProcessed(1);

  // The redirect request should be processed in browser and gets sent.
  request_handlers[1]->WaitForRequest();
  // End the keepalive request by sending back final response.
  request_handlers[1]->Send(k200TextResponse);
  request_handlers[1]->Done();

  // The response should be processed in browser.
  loaders_observer().WaitForTotalOnReceiveResponseProcessed(1);
  // `KeepAliveURLLoader::OnComplete` will not be called but the loader must
  // still be terminated, as renderer is dead.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Delays handling an unsafe redirect for a keepalive ping until after the page
// making the keepalive ping has been unloaded.
// The browser must ensure the unsafe redirect is not followed.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveUnSafeRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char unsafe_redirect_target[] = "chrome://settings";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> chrome://settings
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(method), request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         unsafe_redirect_target));

  // The redirect is unsafe, so the loader is terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed(
      {net::ERR_UNSAFE_REDIRECT});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// Delays handling an violating CSP redirect for a keepalive ping until after
// the page making the keepalive ping has been unloaded.
// The browser must ensure the redirect is not followed.
IN_PROC_BROWSER_TEST_P(KeepAliveURLBrowserTest,
                       ReceiveViolatingCSPRedirectAfterPageUnload) {
  const std::string method = GetParam();
  const char violating_csp_redirect_target[] = "http://b.com/beacon-redirected";
  auto request_handler =
      std::move(RegisterRequestHandlers({kKeepAliveEndpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up redirects according to the following redirect chain:
  // fetch("http://a.com:<port>/beacon", keepalive: true)
  // --> http://b.com/beacon-redirected
  LoadPageWithKeepAliveRequestAndSendResponseAfterUnload(
      GetKeepAlivePageURL(method, /*num_requests=*/1, /*set_csp=*/true),
      request_handler.get(),
      base::StringPrintf("HTTP/1.1 301 Moved Permanently\r\n"
                         "Location: %s\r\n"
                         "\r\n",
                         violating_csp_redirect_target));

  // The redirect doesn't match CSP source from the 1st page, so the loader is
  // terminated.
  loaders_observer().WaitForTotalOnCompleteProcessed({net::ERR_BLOCKED_BY_CSP});
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

class SendBeaconBrowserTestBase : public KeepAliveURLBrowserTestBase {
 protected:
  virtual std::string beacon_payload_type() const = 0;

  GURL GetBeaconPageURL(
      const GURL& beacon_url,
      bool with_non_cors_safelisted_content,
      absl::optional<int> delay_iframe_removal_ms = absl::nullopt) const {
    std::vector<std::string> queries = {
        "/send-beacon-in-iframe.html?url=" + EncodeURL(beacon_url),
        "&payload_type=" + beacon_payload_type()};
    if (with_non_cors_safelisted_content) {
      // Setting the payload's content type to `application/octet-stream`, as
      // only `application/x-www-form-urlencoded`, `multipart/form-data`, and
      // `text/plain` MIME types are allowed for CORS-safelisted `content-type`
      // request header.
      // https://fetch.spec.whatwg.org/#cors-safelisted-request-header
      queries.push_back("&payload_content_type=application/octet-stream");
    }
    if (delay_iframe_removal_ms.has_value()) {
      queries.push_back(base::StringPrintf("&delay_iframe_removal_ms=%d",
                                           delay_iframe_removal_ms.value()));
    }

    return server()->GetURL(kPrimaryHost, base::StrCat(queries));
  }

  // Navigates to a page that calls `navigator.sendBeacon(beacon_url)` from a
  // programmatically created iframe. The iframe will then be removed after the
  // JS call after an optional `delay_iframe_removal_ms` interval.
  // `request_handler` must handle the final URL of the sendBeacon request.
  void LoadPageWithIframeAndSendBeacon(
      const GURL& beacon_url,
      net::test_server::ControllableHttpResponse* request_handler,
      const std::string& response,
      int expect_total_redirects,
      absl::optional<int> delay_iframe_removal_ms = absl::nullopt) {
    // Navigate to the page that calls sendBeacon with `beacon_url` from an
    // appended iframe.
    ASSERT_TRUE(NavigateToURL(
        web_contents(),
        GetBeaconPageURL(beacon_url,
                         /*with_non_cors_safelisted_content=*/false,
                         delay_iframe_removal_ms)));
    ASSERT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // All redirects, if exist, should be processed in browser first.
    loaders_observer().WaitForTotalOnReceiveRedirectProcessed(
        expect_total_redirects);
    // After in-browser processing, the loader should remain alive to support
    // forwarding stored redirects/response to renderer. But it may or may not
    // connect to a renderer.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);

    // Ensure the sendBeacon request is sent.
    request_handler->WaitForRequest();
    // Send back final response to terminate in-browser request handling.
    request_handler->Send(response);
    request_handler->Done();

    // After in-browser redirect/response processing, the in-browser logic may
    // or may not forward redirect/response to renderer process, depending on
    // whether the renderer is still alive.
    loaders_observer().WaitForTotalOnReceiveResponse(1);
    // OnComplete may not be called if the renderer dies too early in before
    // receiving response.

    // The loader should all be gone.
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
  }
};

class SendBeaconBrowserTest
    : public SendBeaconBrowserTestBase,
      public ::testing::WithParamInterface<std::string> {
 protected:
  std::string beacon_payload_type() const override { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    SendBeaconBrowserTest,
    ::testing::Values("string", "arraybuffer", "form", "blob"),
    [](const testing::TestParamInfo<KeepAliveURLBrowserTest::ParamType>& info) {
      return info.param;
    });

// TODO(crbug.com/1482176): Re-enable this test on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MultipleRedirectsRequestWithIframeRemoval \
  DISABLED_MultipleRedirectsRequestWithIframeRemoval
#else
#define MAYBE_MultipleRedirectsRequestWithIframeRemoval \
  MultipleRedirectsRequestWithIframeRemoval
#endif
// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// The JS call happens in an iframe that is removed right after the sendBeacon()
// call, so the chain of redirects & response handling must survive the iframe
// unload.
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MAYBE_MultipleRedirectsRequestWithIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that causes a redirect chain of 4 URLs.
//
// Unlike the `MultipleRedirectsRequestWithIframeRemoval` test case above, the
// request here is fired within an iframe that will be removed shortly
// (delayed by 0ms, roughly in the JS next event cycle).
// This is to mimic the following scenario:
//
// 1. The server returns a redirect.
// 2. In the browser process KeepAliveURLLoader::OnReceiveRedirect(),
//    forwarding_client_ is not null (as renderer/iframe still exists), so it
//    calls forwarding_client_->OnReceiveRedirect() IPC to forward to renderer.
// 3. The renderer process is somehow shut down before its
//    URLLoaderClient::OnReceiveRedirect() is finished, so the redirect chain is
//    incompleted.
// 4. KeepAliveURLLoader::OnRendererConnectionError() is triggered, and only
//    aware of forwarding_client_'s disconnection. It should take over redirect
//    chain handling.
//
// Without delaying iframe removal, renderer disconnection may happen in between
// (2) and (3).
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MultipleRedirectsRequestWithDelayedIframeRemoval) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) URL with CORS-safelisted
  // payload that causes multiple redirects.
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = GetCrossOriginMultipleRedirectsURL(target_url);

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/3,
                                  /*delay_iframe_removal_ms=*/0);
}

// Tests navigator.sendBeacon() with a cross-origin & CORS-safelisted request
// that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should succeed.
// TODO(crbug.com/1485088): Flaky on Android and Mac.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#define MAYBE_CrossOriginAndCORSSafelistedRedirectRequest \
  DISABLED_CrossOriginAndCORSSafelistedRedirectRequest
#else
#define MAYBE_CrossOriginAndCORSSafelistedRedirectRequest \
  CrossOriginAndCORSSafelistedRedirectRequest
#endif
IN_PROC_BROWSER_TEST_P(SendBeaconBrowserTest,
                       MAYBE_CrossOriginAndCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.com:<port>/no-cors-server-redirect-307?...",
  //     <CORS-safelisted payload>)
  // --> http://b.com:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));

  LoadPageWithIframeAndSendBeacon(beacon_url, request_handler.get(),
                                  k200TextResponse,
                                  /*expect_total_redirects=*/1);
}

class SendBeaconBlobBrowserTest : public SendBeaconBrowserTestBase {
 protected:
  std::string beacon_payload_type() const override { return "blob"; }
};

// Tests navigator.sendBeacon() with a cross-origin & non-CORS-safelisted
// request that redirects from url1 to url2. The redirect is handled by a server
// endpoint (/no-cors-server-redirect-307) which does not support CORS.
// As navigator.sendBeacon() marks its request with `no-cors`, the redirect
// should fail.
IN_PROC_BROWSER_TEST_F(SendBeaconBlobBrowserTest,
                       CrossOriginAndNonCORSSafelistedRedirectRequest) {
  const auto beacon_endpoint =
      base::StringPrintf("%s?id=%s", kKeepAliveEndpoint, kBeaconId);
  auto request_handler =
      std::move(RegisterRequestHandlers({beacon_endpoint})[0]);
  ASSERT_TRUE(server()->Start());

  // Set up a cross-origin (kSecondaryHost) redirect with non-CORS-safelisted
  // payload according to the following redirect chain:
  // navigator.sendBeacon(
  //     "http://b.com:<port>/no-cors-server-redirect-307?...",
  //     <non-CORS-safelisted payload>) => should fail here
  // --> http://b.com:<port>/beacon?id=beacon01
  const auto target_url = server()->GetURL(kSecondaryHost, beacon_endpoint);
  const auto beacon_url = server()->GetURL(
      kSecondaryHost, base::StringPrintf("/no-cors-server-redirect-307?%s",
                                         EncodeURL(target_url).c_str()));
  // Navigate to the page that calls sendBeacon with `beacon_url` from an
  // appended iframe, which will be removed shortly after calling sendBeacon().
  ASSERT_TRUE(NavigateToURL(
      web_contents(),
      GetBeaconPageURL(beacon_url, /*with_non_cors_safelisted_content=*/true)));

  // The redirect is rejected in-browser during redirect (with
  // non-CORS-safelisted payload) handling because /no-cors-server-redirect-xxx
  // doesn't support CORS. Thus, KeepAliveURLLoader::OnReceiveRedirect() is not
  // called but KeepAliveURLLoader::OnComplete().
  // Note that renderer can be gone at any point before or after the first URL
  // is loaded. So OnComplete() may or may not be forwarded.
  loaders_observer().WaitForTotalOnComplete({net::ERR_FAILED});
  EXPECT_FALSE(request_handler->has_received_request());
  // After in-browser processing, the loader should all be gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 0u);
}

// A base class to help testing JS fetchLater() API behaviors.
class FetchLaterBrowserTestBase : public KeepAliveURLBrowserTestBase {
 protected:
  void SetUp() override {
    // fetchLater() API only supports HTTPS requests.
    SetUseHttps();
    KeepAliveURLBrowserTestBase::SetUp();
  }

  bool NavigateToURL(const GURL& url) {
    previous_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    bool ret = content::NavigateToURL(web_contents(), url);
    current_document_ =
        std::make_unique<RenderFrameHostImplWrapper>(current_frame_host());
    return ret;
  }
  bool WaitUntilPreviousDocumentDeleted() {
    CHECK(previous_document_);
    // `previous_document_` might already be destroyed here.
    return previous_document_->WaitUntilRenderFrameDeleted();
  }
  // Caution: the returned document might already be killed if BFCache is not
  // working.
  RenderFrameHostImplWrapper& previous_document() {
    CHECK(previous_document_);
    CHECK(!previous_document_->IsDestroyed());
    return *previous_document_;
  }
  RenderFrameHostImplWrapper& current_document() {
    CHECK(previous_document_);
    return *current_document_;
  }

  // Navigates to an empty page, and executes `script` on it.
  void RunScript(const std::string& script) {
    ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
    ASSERT_TRUE(ExecJs(web_contents(), script));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Navigates to a page that executes `script`, and navigates to another page.
  void RunScriptAndNavigateAway(const std::string& script) {
    RunScript(script);

    // Navigate to cross-origin page to ensure the 1st page can be unloaded if
    // BackForwardCache is disabled.
    ASSERT_TRUE(
        NavigateToURL(server()->GetURL(kSecondaryHost, "/title2.html")));
    ASSERT_TRUE(WaitForLoadStop(web_contents()));
  }

  // Expects `total` number of FetchLater requests to be sent.
  // `total` must equal to the size of `request_handlers`.
  // `requester_handlers` are to wait for the FetchLater requests and to
  // respond.
  void ExpectFetchLaterRequests(
      size_t total,
      std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>&
          request_handlers) {
    SCOPED_TRACE(
        base::StringPrintf("ExpectFetchLaterRequests: %zu requests", total));
    ASSERT_EQ(total, request_handlers.size());
    EXPECT_EQ(loader_service()->NumLoadersForTesting(), total);

    for (const auto& handler : request_handlers) {
      // Waits for a FetchLater request.
      handler->WaitForRequest();
      // Sends back final response to terminate in-browser request handling.
      handler->Send(k200TextResponse);
      // Triggers OnComplete.
      handler->Done();
    }

    loaders_observer().WaitForTotalOnReceiveResponse(total);
    // TODO(crbug.com/1356128): Check NumLoadersForTesting==0 after migrating to
    // in-browser ThrottlingURLLoader.
    // Current implementation cannot ensure receiving renderer disconnection.
    // Also need to wait for TotalOnComplete by `total`, not by states.
  }

 private:
  std::unique_ptr<RenderFrameHostImplWrapper> current_document_ = nullptr;
  std::unique_ptr<RenderFrameHostImplWrapper> previous_document_ = nullptr;
};

// A type to support parameterized testing for timeout-related tests.
struct TestTimeoutType {
  std::string test_case_name;
  int32_t timeout;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is off.
//
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
class FetchLaterNoBackForwardCacheBrowserTest
    : public FetchLaterBrowserTestBase,
      public testing::WithParamInterface<TestTimeoutType> {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {{}}}};
    return enabled_features;
  }
  const DisabledFeaturesType& GetDisabledFeatures() override {
    static const DisabledFeaturesType disabled_features = {
        features::kBackForwardCache};
    return disabled_features;
  }
};

INSTANTIATE_TEST_SUITE_P(
    All,
    FetchLaterNoBackForwardCacheBrowserTest,
    testing::ValuesIn<std::vector<TestTimeoutType>>({
        {"LongTimeout", 600000},      // 10 minutes
        {"OneMinuteTimeout", 60000},  // 1 minute
    }),
    [](const testing::TestParamInfo<TestTimeoutType>& info) {
      return info.param.test_case_name;
    });

// All pending FetchLater requests should be sent after the initiator page is
// gone, no matter how much time their activateAfter has left.
// Disables BackForwardCache such that a page is discarded right away on user
// navigating to another page.
IN_PROC_BROWSER_TEST_P(FetchLaterNoBackForwardCacheBrowserTest,
                       SendOnPageDiscardBeforeActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url, target_url});
  ASSERT_TRUE(server()->Start());

  // Creates two FetchLater requests with various long activateAfter, which
  // should all be sent on page discard.
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: $2});
    fetchLater($1, {activateAfter: $2});
  )",
                                     target_url, GetParam().timeout));
  // Ensure the 1st page has been unloaded.
  ASSERT_TRUE(WaitUntilPreviousDocumentDeleted());

  // Loaders are disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 2u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(2, request_handlers);
}

class FetchLaterWithBackForwardCacheMetricsBrowserTestBase
    : public FetchLaterBrowserTestBase,
      public BackForwardCacheMetricsTestMatcher {
 protected:
  void SetUpOnMainThread() override {
    // TestAutoSetUkmRecorder's constructor requires a sequenced context.
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    histogram_tester_ = std::make_unique<base::HistogramTester>();
    FetchLaterBrowserTestBase::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    ukm_recorder_.reset();
    histogram_tester_.reset();
    FetchLaterBrowserTestBase::TearDownOnMainThread();
  }

  // `BackForwardCacheMetricsTestMatcher` implementation.
  const ukm::TestAutoSetUkmRecorder& ukm_recorder() override {
    return *ukm_recorder_;
  }
  const base::HistogramTester& histogram_tester() override {
    return *histogram_tester_;
  }

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

// Tests to cover FetchLater's behaviors when BackForwardCache is on but does
// not come into play.
//
// Setting long `BackForwardCache TTL (1min)` so that FetchLater sending cannot
// be caused by page eviction out of BackForwardCache.
class FetchLaterNoActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// A pending FetchLater request with default options should be sent after the
// initiator page is gone.
// Similar to SendOnPageDiscardBeforeActivationTimeout.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       SendOnPageDeletion) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request in an iframe, which is removed after loaded.
  ASSERT_TRUE(NavigateToURL(
      server()->GetURL(kPrimaryHost, "/page_with_blank_iframe.html")));
  ASSERT_TRUE(ExecJs(web_contents(), R"(
    var promise = new Promise(resolve => {
      window.addEventListener('message', e => {
        const iframe = document.getElementById('test_iframe');
        iframe.remove();
        resolve(e.data);
      });
    });
  )"));
  auto* iframe =
      static_cast<RenderFrameHostImpl*>(ChildFrameAt(web_contents(), 0));
  EXPECT_TRUE(ExecJs(iframe, JsReplace(R"(
      fetchLater($1);
      window.parent.postMessage(true, "*");
    )",
                                       target_url)));
  // `iframe` is removed after it calls fetchLater().
  EXPECT_EQ(true, EvalJs(web_contents(), "promise"));

  // The loader is disconnected after the 1st page is gone.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 1u);
  // The FetchLater requests should've been sent after the 1st page is gone.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should not be sent after its page gets restored
// from BackForwardCache before getting evicted.
IN_PROC_BROWSER_TEST_F(
    FetchLaterNoActivationTimeoutBrowserTest,
    NotSendWhenPageIsRestoredBeforeBeingEvictedFromBackForwardCache) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1);
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Navigate back to the 1st page.
  ASSERT_TRUE(HistoryGoBack(web_contents()));

  // The same page is still alive.
  ExpectRestored(FROM_HERE);
  // The loader should still exist, but the request should not be sent.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Without an activateAfter set, a pending FetchLater request should not be
// sent out during its page frozen state.
// Similar to ResetActivationTimeoutTimerOnPageResume.
IN_PROC_BROWSER_TEST_F(FetchLaterNoActivationTimeoutBrowserTest,
                       NotSendWhenPageIsResumedAfterBeingFrozen) {
  const std::string target_url = kFetchLaterEndpoint;
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with NO activateAfter.
  // It should be impossible to send out during page frozen.
  ASSERT_TRUE(NavigateToURL(server()->GetURL(kPrimaryHost, "/title1.html")));
  ASSERT_TRUE(ExecJs(web_contents(), JsReplace(R"(
    fetchLater($1);
  )",
                                               target_url)));
  ASSERT_TRUE(WaitForLoadStop(web_contents()));

  // Forces to freeze the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(true);

  // The FetchLater request should not be sent.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);

  // Forces to wake up the current page.
  web_contents()->WasHidden();
  web_contents()->SetPageFrozen(false);
  // The FetchLater request should not be sent.
  // TODO(crbug.com/1465781): Verify FetchLaterResult once
  // https://crrev.com/c/4820528 is submitted.
  EXPECT_EQ(loader_service()->NumLoadersForTesting(), 1u);
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
}

// Tests to cover FetchLater's activateAfter behaviors when BackForwardCache
// is on and may come into play.
//
// BackForwardCache eviction is simulated by calling
// `DisableBFCacheForRFHForTesting(previous_document())` instead of relying on
// its TTL.
class FetchLaterActivationTimeoutBrowserTest
    : public FetchLaterWithBackForwardCacheMetricsBrowserTestBase {
 protected:
  const FeaturesType& GetEnabledFeatures() override {
    static const FeaturesType enabled_features = {
        {blink::features::kFetchLaterAPI, {}},
        {features::kBackForwardCache, {{}}},
        // Sets to a long timeout, as tests below should not rely on it.
        {features::kBackForwardCacheTimeToLiveControl,
         {{"time_to_live_seconds", "60"}}},
        // Forces BackForwardCache to work in low memory device.
        {features::kBackForwardCacheMemoryControls,
         {{"memory_threshold_for_back_forward_cache_in_mb", "0"}}}};
    return enabled_features;
  }
};

// When setting activateAfter>0, a pending FetchLater request should be sent
// after around the specified time, if no navigation happens.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnActivationTimeout) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with activateAfter=2s.
  // It should be sent out after 2s.
  RunScript(JsReplace(R"(
    fetchLater($1, {activateAfter: 2000});
  )",
                      target_url));
  ASSERT_FALSE(current_document().IsDestroyed());

  // The loader should still exist as the page exists.
  EXPECT_EQ(loader_service()->NumDisconnectedLoadersForTesting(), 0u);
  // The FetchLater request should be sent, triggered by its activateAfter.
  ExpectFetchLaterRequests(1, request_handlers);
}

// A pending FetchLater request should be sent when its page is evicted out of
// BackForwardCache.
IN_PROC_BROWSER_TEST_F(FetchLaterActivationTimeoutBrowserTest,
                       SendOnBackForwardCachedEviction) {
  const std::string target_url = kFetchLaterEndpoint;
  auto request_handlers = RegisterRequestHandlers({target_url});
  ASSERT_TRUE(server()->Start());

  // Creates a FetchLater request with long activateAfter (3min)
  RunScriptAndNavigateAway(JsReplace(R"(
    fetchLater($1, {activateAfter: 180000});
  )",
                                     target_url));
  ASSERT_TRUE(previous_document()->IsInBackForwardCache());
  // Forces evicting previous page. This will also post a task that destroys it.
  DisableBFCacheForRFHForTesting(previous_document()->GetGlobalId());
  ASSERT_TRUE(previous_document()->is_evicted_from_back_forward_cache());
  // Eviction happens immediately, but RFH deletion may be delayed.
  ASSERT_TRUE(previous_document().WaitUntilRenderFrameDeleted());

  // The loader is disconnected after the page is evicted by browser process to
  // start loading the request. However, it may happen earlier or later, so it's
  // difficult to assert the existence of the disconnected loader.

  // At the end, the FetchLater request should be sent, and the loader is
  // expected to process the response.
  ExpectFetchLaterRequests(1, request_handlers);
}

// All other send-on-BFCache behaviors are covered in
// send-on-deactivate.tentative.https.window.js

}  // namespace content
