// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "chrome/browser/dips/dips_bounce_detector.h"

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_service_factory.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/dips/dips_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/metrics_proto/ukm/source.pb.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

using base::Bucket;
using content::NavigationHandle;
using content::WebContents;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;
using ukm::builders::DIPS_Redirect;

namespace {

// Returns a simplified URL representation for ease of comparison in tests.
// Just host+path.
std::string FormatURL(const GURL& url) {
  return base::StrCat({url.host_piece(), url.path_piece()});
}

void AppendRedirect(std::vector<std::string>* redirects,
                    const DIPSRedirectInfo& redirect,
                    const DIPSRedirectChainInfo& chain) {
  redirects->push_back(base::StringPrintf(
      "[%d/%d] %s -> %s (%s) -> %s", redirect.index + 1, chain.length,
      FormatURL(chain.initial_url).c_str(), FormatURL(redirect.url).c_str(),
      CookieAccessTypeToString(redirect.access_type).data(),
      FormatURL(chain.final_url).c_str()));
}

void AppendRedirects(std::vector<std::string>* vec,
                     std::vector<DIPSRedirectInfoPtr> redirects,
                     DIPSRedirectChainInfoPtr chain) {
  for (const auto& redirect : redirects) {
    AppendRedirect(vec, *redirect, *chain);
  }
}

void AppendSitesInReport(std::vector<std::string>* reports,
                         const std::set<std::string>& sites) {
  reports->push_back(base::JoinString(
      std::vector<base::StringPiece>(sites.begin(), sites.end()), ", "));
}

}  // namespace

// Keeps a log of DidStartNavigation, OnCookiesAccessed, and DidFinishNavigation
// executions.
class WCOCallbackLogger
    : public content::WebContentsObserver,
      public content::WebContentsUserData<WCOCallbackLogger> {
 public:
  WCOCallbackLogger(const WCOCallbackLogger&) = delete;
  WCOCallbackLogger& operator=(const WCOCallbackLogger&) = delete;

  const std::vector<std::string>& log() const { return log_; }

 private:
  explicit WCOCallbackLogger(content::WebContents* web_contents);
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<WCOCallbackLogger>;

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

  std::vector<std::string> log_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WCOCallbackLogger::WCOCallbackLogger(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<WCOCallbackLogger>(*web_contents) {}

void WCOCallbackLogger::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  log_.push_back(
      base::StringPrintf("DidStartNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  // Callbacks for favicons are ignored only in testing logs because their
  // ordering is variable and would cause flakiness
  if (details.url.path() == "/favicon.ico") {
    return;
  }

  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(RenderFrameHost, %s: %s)",
      details.type == content::CookieAccessDetails::Type::kChange ? "Change"
                                                                  : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::OnCookiesAccessed(
    NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  log_.push_back(base::StringPrintf(
      "OnCookiesAccessed(NavigationHandle, %s: %s)",
      details.type == content::CookieAccessDetails::Type::kChange ? "Change"
                                                                  : "Read",
      FormatURL(details.url).c_str()));
}

void WCOCallbackLogger::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // Android testing produces callbacks for a finished navigation to "blank" at
  // the beginning of a test. These should be ignored here.
  if (FormatURL(navigation_handle->GetURL()) == "blank" ||
      navigation_handle->GetPreviousPrimaryMainFrameURL().is_empty()) {
    return;
  }
  log_.push_back(
      base::StringPrintf("DidFinishNavigation(%s)",
                         FormatURL(navigation_handle->GetURL()).c_str()));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

class DIPSBounceDetectorBrowserTest : public PlatformBrowserTest {
 protected:
  DIPSBounceDetectorBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            // TODO(crbug.com/1394910): Use HTTPS URLs in tests to avoid having
            // to disable this feature.
            features::kHttpsUpgrades,
        });
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("sub.b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");
    host_resolver()->AddRule("d.test", "127.0.0.1");
    host_resolver()->AddRule("e.test", "127.0.0.1");
    host_resolver()->AddRule("f.test", "127.0.0.1");
    host_resolver()->AddRule("g.test", "127.0.0.1");
    web_contents_observer_ =
        DIPSWebContentsObserver::FromWebContents(GetActiveWebContents());
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  void StartAppendingRedirectsTo(std::vector<std::string>* redirects) {
    web_contents_observer_->SetRedirectChainHandlerForTesting(
        base::BindRepeating(&AppendRedirects, redirects));
  }

  void StartAppendingReportsTo(std::vector<std::string>* reports) {
    web_contents_observer_->SetIssueReportingCallbackForTesting(
        base::BindRepeating(&AppendSitesInReport, reports));
  }

  void BlockUntilHelperProcessesPendingRequests() {
    base::SequenceBound<DIPSStorage>* storage =
        DIPSServiceFactory::GetForBrowserContext(
            GetActiveWebContents()->GetBrowserContext())
            ->storage();
    storage->FlushPostedTasksForTesting();
  }

  void StateForURL(const GURL& url, StateForURLCallback callback) {
    DIPSService* dips_service = DIPSServiceFactory::GetForBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
    dips_service->storage()
        ->AsyncCall(&DIPSStorage::Read)
        .WithArgs(url)
        .Then(std::move(callback));
  }

  absl::optional<StateValue> GetDIPSState(const GURL& url) {
    absl::optional<StateValue> state;

    StateForURL(url, base::BindLambdaForTesting([&](DIPSState loaded_state) {
                  if (loaded_state.was_loaded()) {
                    state = loaded_state.ToStateValue();
                  }
                }));
    BlockUntilHelperProcessesPendingRequests();

    return state;
  }

  // Navigate to /set-cookie on `host` and wait for OnCookiesAccessed() to be
  // called.
  [[nodiscard]] bool NavigateToSetCookie(base::StringPiece host) {
    auto* web_contents = GetActiveWebContents();
    const auto url =
        embedded_test_server()->GetURL(host, "/set-cookie?name=value");
    URLCookieAccessObserver observer(web_contents, url,
                                     URLCookieAccessObserver::Type::kChange);
    bool success = content::NavigateToURL(web_contents, url);
    if (success) {
      observer.Wait();
    }
    return success;
  }

  void CreateImageAndWaitForCookieAccess(const GURL& image_url) {
    WebContents* web_contents = GetActiveWebContents();
    URLCookieAccessObserver observer(web_contents, image_url,
                                     URLCookieAccessObserver::Type::kRead);
    ASSERT_TRUE(content::ExecJs(web_contents,
                                content::JsReplace(
                                    R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                    image_url),
                                content::EXECUTE_SCRIPT_NO_USER_GESTURE));
    // The image must cause a cookie access, or else this will hang.
    observer.Wait();
  }

  // Perform a browser-based navigation to terminate the current redirect chain.
  // (NOTE: tests using WCOCallbackLogger must call this *after* checking the
  // log, since this navigation will be logged.)
  void EndRedirectChain() {
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        embedded_test_server()->GetURL("a.test", "/title1.html")));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  raw_ptr<DIPSWebContentsObserver, DanglingUntriaged> web_contents_observer_ =
      nullptr;
};

// The timing of WCO::OnCookiesAccessed() execution is unpredictable for
// redirects. Sometimes it's called before WCO::DidRedirectNavigation(), and
// sometimes after. Therefore DIPSBounceDetector needs to know when it's safe to
// judge an HTTP redirect as stateful (accessing cookies) or not. This test
// tries to verify that OnCookiesAccessed() is always called before
// DidFinishNavigation(), so that DIPSBounceDetector can safely perform that
// judgement in DidFinishNavigation().
//
// This test also verifies that OnCookiesAccessed() is called for URLs in the
// same order that they're visited (and that for redirects that both read and
// write cookies, OnCookiesAccessed() is called with kRead before it's called
// with kChange, although DIPSBounceDetector doesn't depend on that anymore.)
//
// If either assumption is incorrect, this test will be flaky. On 2022-04-27 I
// (rtarpine) ran this test 1000 times in 40 parallel jobs with no failures, so
// it seems robust.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       AllCookieCallbacksBeforeNavigationFinished) {
  GURL redirect_url = embedded_test_server()->GetURL(
      "a.test",
      "/cross-site/b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
      "d.test/set-cookie?name=value");
  GURL final_url =
      embedded_test_server()->GetURL("d.test", "/set-cookie?name=value");
  content::WebContents* web_contents = GetActiveWebContents();

  // Set cookies on all 4 test domains
  ASSERT_TRUE(NavigateToSetCookie("a.test"));
  ASSERT_TRUE(NavigateToSetCookie("b.test"));
  ASSERT_TRUE(NavigateToSetCookie("c.test"));
  ASSERT_TRUE(NavigateToSetCookie("d.test"));

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Visit the redirect.
  URLCookieAccessObserver observer(web_contents, final_url,
                                   URLCookieAccessObserver::Type::kChange);
  ASSERT_TRUE(content::NavigateToURL(web_contents, redirect_url, final_url));
  observer.Wait();

  // Verify that the 7 OnCookiesAccessed() executions are called in order, and
  // all between DidStartNavigation() and DidFinishNavigation().
  //
  // Note: according to web_contents_observer.h, sometimes cookie reads/writes
  // from navigations may cause the RenderFrameHost* overload of
  // OnCookiesAccessed to be called instead. We haven't seen that yet, and this
  // test will intentionally fail if it happens so that we'll notice.
  EXPECT_THAT(
      logger->log(),
      testing::ContainerEq(std::vector<std::string>(
          {("DidStartNavigation(a.test/cross-site/b.test/"
            "cross-site-with-cookie/"
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "a.test/cross-site/b.test/cross-site-with-cookie/c.test/"
            "cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "b.test/cross-site-with-cookie/c.test/cross-site-with-cookie/"
            "d.test/"
            "set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Read: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           ("OnCookiesAccessed(NavigationHandle, Change: "
            "c.test/cross-site-with-cookie/d.test/set-cookie)"),
           "OnCookiesAccessed(NavigationHandle, Read: d.test/set-cookie)",
           "OnCookiesAccessed(NavigationHandle, Change: d.test/set-cookie)",
           "DidFinishNavigation(d.test/set-cookie)"})));
}

// An EmbeddedTestServer request handler for
// /cross-site-with-samesite-none-cookie URLs. Like /cross-site-with-cookie, but
// the cookie has additional Secure and SameSite=None attributes.
std::unique_ptr<net::test_server::HttpResponse>
HandleCrossSiteSameSiteNoneCookieRedirect(
    net::EmbeddedTestServer* server,
    const net::test_server::HttpRequest& request) {
  const std::string prefix = "/cross-site-with-samesite-none-cookie";
  if (!net::test_server::ShouldHandle(request, prefix)) {
    return nullptr;
  }

  std::string dest_all = base::UnescapeBinaryURLComponent(
      request.relative_url.substr(prefix.size() + 1));

  std::string dest;
  size_t delimiter = dest_all.find("/");
  if (delimiter != std::string::npos) {
    dest = base::StringPrintf(
        "//%s:%hu/%s", dest_all.substr(0, delimiter).c_str(), server->port(),
        dest_all.substr(delimiter + 1).c_str());
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", dest);
  http_response->AddCustomHeader("Set-Cookie",
                                 "server-redirect=true; Secure; SameSite=None");
  http_response->set_content_type("text/html");
  http_response->set_content(base::StringPrintf(
      "<html><head></head><body>Redirecting to %s</body></html>",
      dest.c_str()));
  return http_response;
}

// Ignore iframes because their state will be partitioned under the top-level
// site anyway.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       IgnoreServerRedirectsInIframes) {
  // We host the iframe content on an HTTPS server, because for it to write a
  // cookie, the cookie needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  const GURL root_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const GURL redirect_url = https_server.GetURL(
      "b.test", "/cross-site-with-samesite-none-cookie/c.test/title1.html");
  const std::string iframe_id = "test";
  content::WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  ASSERT_TRUE(content::NavigateToURL(web_contents, root_url));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents, iframe_id, redirect_url));
  EndRedirectChain();

  // b.test had a stateful redirect, but because it was in an iframe, we ignored
  // it.
  EXPECT_THAT(redirects, IsEmpty());
}

// This test verifies that sites in a redirect chain with previous user
// interaction are not reported in the resulting issue when a navigation
// finishes.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       ReportRedirectorsInChain_OmitSitesWithInteraction) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> reports;
  StartAppendingReportsTo(&reports);

  // Record user activation on d.test.
  GURL url = embedded_test_server()->GetURL("d.test", "/title1.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, url));
  UserActivationObserver observer(web_contents,
                                  web_contents->GetPrimaryMainFrame());
  content::WaitForHitTestData(web_contents->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents, 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();

  // Verify interaction was recorded for d.test, before proceeding.
  absl::optional<StateValue> state = GetDIPSState(url);
  ASSERT_TRUE(state.has_value());
  ASSERT_TRUE(state->user_interaction_times.has_value());

  // Visit initial page on a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test, which S-redirects to g.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL(
          "e.test", "/cross-site/f.test/cross-site/g.test/title1.html"),
      embedded_test_server()->GetURL("g.test", "/title1.html")));
  EndRedirectChain();
  BlockUntilHelperProcessesPendingRequests();

  EXPECT_THAT(reports, ElementsAre(("b.test"), ("c.test"), ("e.test, f.test")));
}

// This test verifies that a third-party cookie access doesn't cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(
    DIPSBounceDetectorBrowserTest,
    DetectStatefulRedirect_Client_IgnoreThirdPartySubresource) {
  // We host the image on an HTTPS server, because for it to read a third-party
  // cookie, it needs to be SameSite=None and Secure.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
  https_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  https_server.RegisterDefaultHandler(base::BindRepeating(
      &HandleCrossSiteSameSiteNoneCookieRedirect, &https_server));
  ASSERT_TRUE(https_server.Start());

  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url = https_server.GetURL("d.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set SameSite=None cookie on d.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, https_server.GetURL(
                        "d.test", "/set-cookie?foo=bar;Secure;SameSite=None")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a third-party cookie read.
  CreateImageAndWaitForCookieAccess(image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on d.test
                  ("DidStartNavigation(d.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: d.test/set-cookie)"),
                  ("DidFinishNavigation(d.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading third-party d.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: d.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain();

  // b.test is a bounce, but not stateful.
  EXPECT_THAT(redirects, ElementsAre("[1/1] a.test/title1.html"
                                     " -> b.test/title1.html (None)"
                                     " -> c.test/title1.html"));
}

// This test verifies that a same-site cookie access DOES cause a client
// bounce to be considered stateful.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_Client_FirstPartySubresource) {
  GURL initial_url = embedded_test_server()->GetURL("a.test", "/title1.html");
  GURL bounce_url = embedded_test_server()->GetURL("b.test", "/title1.html");
  GURL final_url = embedded_test_server()->GetURL("c.test", "/title1.html");
  GURL image_url =
      embedded_test_server()->GetURL("sub.b.test", "/favicon/icon.png");
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Start logging WebContentsObserver callbacks.
  WCOCallbackLogger::CreateForWebContents(web_contents);
  auto* logger = WCOCallbackLogger::FromWebContents(web_contents);

  // Set cookie on sub.b.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents,
      embedded_test_server()->GetURL("sub.b.test", "/set-cookie?foo=bar")));

  // Visit initial page
  ASSERT_TRUE(content::NavigateToURL(web_contents, initial_url));
  // Navigate with a click (not a redirect).
  ASSERT_TRUE(content::NavigateToURLFromRenderer(web_contents, bounce_url));

  // Cause a same-site cookie read.
  CreateImageAndWaitForCookieAccess(image_url);
  // Navigate without a click (i.e. by redirecting).
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(web_contents,
                                                                   final_url));

  EXPECT_THAT(logger->log(),
              ElementsAre(
                  // Set cookie on sub.b.test
                  ("DidStartNavigation(sub.b.test/set-cookie)"),
                  ("OnCookiesAccessed(NavigationHandle, "
                   "Change: sub.b.test/set-cookie)"),
                  ("DidFinishNavigation(sub.b.test/set-cookie)"),
                  // Visit a.test
                  ("DidStartNavigation(a.test/title1.html)"),
                  ("DidFinishNavigation(a.test/title1.html)"),
                  // Bounce on b.test (reading same-site sub.b.test cookie)
                  ("DidStartNavigation(b.test/title1.html)"),
                  ("DidFinishNavigation(b.test/title1.html)"),
                  ("OnCookiesAccessed(RenderFrameHost, "
                   "Read: sub.b.test/favicon/icon.png)"),
                  // Land on c.test
                  ("DidStartNavigation(c.test/title1.html)"),
                  ("DidFinishNavigation(c.test/title1.html)")));
  EndRedirectChain();

  // b.test IS considered a stateful bounce, even though the cookie was read by
  // an image hosted on sub.b.test.
  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> b.test/title1.html "
                           "(Read) -> c.test/title1.html")));
}

// This test verifies that consecutive redirect chains are combined into one.
IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ServerClientClientServer) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to d.test
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL("e.test",
                                     "/cross-site/f.test/title1.html"),
      embedded_test_server()->GetURL("f.test", "/title1.html")));
  EndRedirectChain();

  EXPECT_THAT(
      redirects,
      ElementsAre(("[1/4] a.test/title1.html -> "
                   "b.test/cross-site/c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[2/4] a.test/title1.html -> c.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[3/4] a.test/title1.html -> d.test/title1.html (None) -> "
                   "f.test/title1.html"),
                  ("[4/4] a.test/title1.html -> "
                   "e.test/cross-site/f.test/title1.html (None) -> "
                   "f.test/title1.html")));
}

IN_PROC_BROWSER_TEST_F(DIPSBounceDetectorBrowserTest,
                       DetectStatefulRedirect_ClosingTabEndsChain) {
  WebContents* web_contents = GetActiveWebContents();

  std::vector<std::string> redirects;
  StartAppendingRedirectsTo(&redirects);

  // Visit initial page on a.test
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));

  EXPECT_THAT(redirects, IsEmpty());

  content::WebContentsDestroyedWatcher destruction_watcher(web_contents);
  web_contents->Close();
  destruction_watcher.Wait();

  EXPECT_THAT(redirects,
              ElementsAre(("[1/1] a.test/title1.html -> "
                           "b.test/cross-site/c.test/title1.html (None) -> "
                           "c.test/title1.html")));
}

class DIPSBounceTrackingDevToolsIssueTest
    : public content::TestDevToolsProtocolClient,
      public DIPSBounceDetectorBrowserTest {
 protected:
  void WaitForIssueAndCheckTrackingSites(
      const std::vector<std::string>& sites) {
    auto is_dips_issue = [](const base::Value::Dict& params) {
      return *(params.FindStringByDottedPath("issue.code")) ==
             "BounceTrackingIssue";
    };

    // Wait for notification of a Bounce Tracking Issue.
    base::Value::Dict params = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(is_dips_issue));
    ASSERT_EQ(*params.FindStringByDottedPath("issue.code"),
              "BounceTrackingIssue");

    base::Value::Dict* bounce_tracking_issue_details =
        params.FindDictByDottedPath("issue.details.bounceTrackingIssueDetails");
    ASSERT_TRUE(bounce_tracking_issue_details);

    std::vector<std::string> tracking_sites;
    base::Value::List* tracking_sites_list =
        bounce_tracking_issue_details->FindList("trackingSites");
    if (tracking_sites_list) {
      for (const auto& val : *tracking_sites_list) {
        tracking_sites.push_back(val.GetString());
      }
    }

    // Verify the reported tracking sites match the expected sites.
    EXPECT_THAT(tracking_sites, testing::ElementsAreArray(sites));

    // Clear existing notifications so subsequent calls don't fail by checking
    // `sites` against old notifications.
    ClearNotifications();
  }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    DIPSBounceDetectorBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(DIPSBounceTrackingDevToolsIssueTest,
                       BounceTrackingDevToolsIssue) {
  WebContents* web_contents = GetActiveWebContents();

  // Visit initial page on a.test.
  ASSERT_TRUE(content::NavigateToURL(
      web_contents, embedded_test_server()->GetURL("a.test", "/title1.html")));

  // Open DevTools and enable Audit domain.
  AttachToWebContents(web_contents);
  SendCommandSync("Audits.enable");
  ClearNotifications();

  // Navigate with a click (not a redirect) to b.test, which S-redirects to
  // c.test.
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      web_contents,
      embedded_test_server()->GetURL("b.test",
                                     "/cross-site/c.test/title1.html"),
      embedded_test_server()->GetURL("c.test", "/title1.html")));
  WaitForIssueAndCheckTrackingSites({"b.test"});

  // Navigate without a click (i.e. by C-redirecting) to d.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents, embedded_test_server()->GetURL("d.test", "/title1.html")));
  WaitForIssueAndCheckTrackingSites({"c.test"});

  // Navigate without a click (i.e. by C-redirecting) to e.test, which
  // S-redirects to f.test, which S-redirects to g.test.
  ASSERT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      web_contents,
      embedded_test_server()->GetURL(
          "e.test", "/cross-site/f.test/cross-site/g.test/title1.html"),
      embedded_test_server()->GetURL("g.test", "/title1.html")));
  WaitForIssueAndCheckTrackingSites({"d.test", "e.test", "f.test"});
}
