// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "content/public/browser/browsing_data_remover.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "content/browser/browsing_data/shared_storage_clear_site_data_tester.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_filter_builder.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browsing_data_remover_test_util.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/simple_url_loader_test_helper.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace {

const char kHstsPath[] = "/hsts";
const char kHttpAuthPath[] = "/http_auth";
const char kHstsResponseBody[] = "HSTS set";

std::unique_ptr<net::test_server::HttpResponse> HandleHstsRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url == kHstsPath) {
    std::unique_ptr<net::test_server::BasicHttpResponse> hsts_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    hsts_response->AddCustomHeader("Strict-Transport-Security",
                                   "max-age=1000000");
    hsts_response->set_content(kHstsResponseBody);
    return hsts_response;
  }
  return nullptr;
}

// Handles |request| to "/http_auth". If "Authorization" header is present,
// responds with a non-empty HTTP 200 page (regardless of auth credentials).
// Otherwise serves a Basic Auth challenge.
std::unique_ptr<net::test_server::HttpResponse> HandleHttpAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kHttpAuthPath)
    return nullptr;

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  if (base::Contains(request.headers, "Authorization")) {
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("Success!");
  } else {
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader("WWW-Authenticate",
                                   "Basic realm=\"test realm\"");
  }
  return http_response;
}

}  // namespace

namespace content {

class BrowsingDataRemoverImplBrowserTest : public ContentBrowserTest {
 public:
  BrowsingDataRemoverImplBrowserTest()
      : ssl_server_(net::test_server::EmbeddedTestServer::TYPE_HTTPS) {
    // Use localhost instead of 127.0.0.1, as HSTS isn't allowed on IPs.
    ssl_server_.SetSSLConfig(
        net::test_server::EmbeddedTestServer::CERT_COMMON_NAME_IS_DOMAIN);
    ssl_server_.AddDefaultHandlers(GetTestDataFilePath());
    ssl_server_.RegisterRequestHandler(base::BindRepeating(&HandleHstsRequest));
    ssl_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleHttpAuthRequest));
    EXPECT_TRUE(ssl_server_.Start());
  }

  void SetUpOnMainThread() override {}

  void RemoveAndWait(uint64_t remove_mask) {
    content::BrowsingDataRemover* remover =
        shell()->web_contents()->GetBrowserContext()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  void RemoveWithFilterAndWait(
      uint64_t remove_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter) {
    content::BrowsingDataRemover* remover =
        shell()->web_contents()->GetBrowserContext()->GetBrowsingDataRemover();
    content::BrowsingDataRemoverCompletionObserver completion_observer(remover);
    remover->RemoveWithFilterAndReply(
        base::Time(), base::Time::Max(), remove_mask,
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB,
        std::move(filter), &completion_observer);
    completion_observer.BlockUntilCompletion();
  }

  // Issues a request for kHstsPath on localhost, and expects it to enable HSTS
  // for the domain.
  void IssueRequestThatSetsHsts() {
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = ssl_server_.GetURL("localhost", kHstsPath);

    SimpleURLLoaderTestHelper loader_helper;
    std::unique_ptr<network::SimpleURLLoader> loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory(), loader_helper.GetCallback());
    loader_helper.WaitForCallback();
    ASSERT_TRUE(loader_helper.response_body());
    EXPECT_EQ(kHstsResponseBody, *loader_helper.response_body());

    EXPECT_TRUE(IsHstsSet());
  }

  // Returns true if HSTS is set on localhost.  Does this by issuing an HTTP
  // request to the embedded test server, and expecting it to be redirected from
  // HTTP to HTTPS if HSTS is enabled.  If the request succeeds, it was sent
  // over HTTPS, so HSTS is enabled. If it fails, the request was send using
  // HTTP instead, so HSTS is not enabled for the domain.
  bool IsHstsSet() {
    GURL url = ssl_server_.GetURL("localhost", "/echo");
    GURL::Replacements replacements;
    replacements.SetSchemeStr("http");
    url = url.ReplaceComponents(replacements);
    std::unique_ptr<network::ResourceRequest> request =
        std::make_unique<network::ResourceRequest>();
    request->url = url;

    std::unique_ptr<network::SimpleURLLoader> loader =
        network::SimpleURLLoader::Create(std::move(request),
                                         TRAFFIC_ANNOTATION_FOR_TESTS);
    SimpleURLLoaderTestHelper loader_helper;
    loader->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory(), loader_helper.GetCallback());
    loader_helper.WaitForCallback();

    // On success, HSTS was enabled for the domain.
    if (loader_helper.response_body()) {
      EXPECT_EQ("Echo", *loader_helper.response_body());
      return true;
    }

    // On failure, the server just hangs up, since it didn't receive an SSL
    // handshake.
    EXPECT_EQ(net::ERR_EMPTY_RESPONSE, loader->NetError());
    return false;
  }

  // Sets HTTP auth cache by making a request with credentials specified in the
  // URL to a page with an auth challenge.
  void IssueRequestThatSetsHttpAuthCache() {
    GURL url = ssl_server_.GetURL(kHttpAuthPath);
    GURL::Replacements replacements;
    replacements.SetUsernameStr("user");
    replacements.SetPasswordStr("password");
    GURL url_with_creds = url.ReplaceComponents(replacements);
    ASSERT_TRUE(NavigateToURL(shell(), url_with_creds));

    ASSERT_TRUE(IsHttpAuthCacheSet());
  }

  // Determines if auth cache is populated by seeing if a request to a page with
  // an auth challenge succeeds.
  bool IsHttpAuthCacheSet() {
    // Set a login request callback to be used instead of a login dialog since
    // such a dialog is difficult to control programmatically and doesn't work
    // on all platforms.
    bool login_requested = false;
    ShellContentBrowserClient::Get()->set_login_request_callback(
        base::BindLambdaForTesting(
            [&](bool is_primary_main_frame /* unused */) {
              login_requested = true;
            }));

    GURL url = ssl_server_.GetURL(kHttpAuthPath);
    bool navigation_suceeded = NavigateToURL(shell(), url);

    // Because our login request callback does nothing, navigation should
    // succeed iff login is not needed unless some other unexpected error
    // occurs.
    EXPECT_NE(navigation_suceeded, login_requested);

    return !login_requested && navigation_suceeded;
  }

  network::mojom::URLLoaderFactory* url_loader_factory() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess()
        .get();
  }

  network::mojom::NetworkContext* network_context() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition()
        ->GetNetworkContext();
  }

 private:
  net::test_server::EmbeddedTestServer ssl_server_;
};

// Verify that TransportSecurityState data is cleared for REMOVE_CACHE.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       ClearTransportSecurityState) {
  IssueRequestThatSetsHsts();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_CACHE);
  EXPECT_FALSE(IsHstsSet());
}

// Verify that TransportSecurityState data is not cleared if REMOVE_CACHE is not
// set or there is a deletelist filter.
// TODO(crbug.com/1040065): Add support for filtered deletions and update test.
IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       PreserveTransportSecurityState) {
  IssueRequestThatSetsHsts();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  EXPECT_TRUE(IsHstsSet());

  auto filter = BrowsingDataFilterBuilder::Create(
      BrowsingDataFilterBuilder::Mode::kDelete);
  filter->AddRegisterableDomain("foobar.com");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_CACHE,
                          std::move(filter));
  EXPECT_TRUE(IsHstsSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest, ClearHttpAuthCache) {
  ASSERT_FALSE(IsHttpAuthCacheSet());
  IssueRequestThatSetsHttpAuthCache();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES);
  EXPECT_FALSE(IsHttpAuthCacheSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       PreserveHttpAuthCache) {
  ASSERT_FALSE(IsHttpAuthCacheSet());
  IssueRequestThatSetsHttpAuthCache();

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_DOWNLOADS);
  EXPECT_TRUE(IsHttpAuthCacheSet());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplBrowserTest,
                       ClearHttpAuthCacheWhenEmpty) {
  ASSERT_FALSE(IsHttpAuthCacheSet());

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES);
  EXPECT_FALSE(IsHttpAuthCacheSet());
}

class CookiesBrowsingDataRemoverImplBrowserTest
    : public BrowsingDataRemoverImplBrowserTest {
 public:
  CookiesBrowsingDataRemoverImplBrowserTest() {
    feature_list_.InitAndEnableFeature(net::features::kPartitionedCookies);
  }

  void SetUpOnMainThread() override {
    network_context()->GetCookieManager(
        cookie_manager_.BindNewPipeAndPassReceiver());
  }

  bool SetCookie(
      const GURL& url,
      const std::string& cookie_line,
      const absl::optional<net::CookiePartitionKey>& cookie_partition_key) {
    auto cookie_obj = net::CanonicalCookie::Create(
        url, cookie_line, base::Time::Now(), /*server_time=*/absl::nullopt,
        cookie_partition_key);

    base::test::TestFuture<net::CookieAccessResult> future;
    cookie_manager_->SetCanonicalCookie(*cookie_obj, url,
                                        net::CookieOptions::MakeAllInclusive(),
                                        future.GetCallback());
    return future.Take().status.IsInclude();
  }

  net::CookieList GetAllCookies() {
    base::test::TestFuture<const net::CookieList&> future;
    cookie_manager_->GetAllCookies(future.GetCallback());
    return future.Take();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  mojo::Remote<network::mojom::CookieManager> cookie_manager_;
};

IN_PROC_BROWSER_TEST_F(CookiesBrowsingDataRemoverImplBrowserTest,
                       ClearsAllCookiesByDefault) {
  // Set unpartitioned cookies.
  ASSERT_TRUE(SetCookie(GURL("http://a.com"), "A=0", absl::nullopt));
  ASSERT_TRUE(SetCookie(GURL("https://a.com"), "B=1; secure; samesite=none",
                        absl::nullopt));
  ASSERT_TRUE(SetCookie(GURL("https://b.com"),
                        "C=2; secure; samesite=none; max-age=10000",
                        absl::nullopt));
  ASSERT_EQ(3u, GetAllCookies().size());

  // Set partitioned cookies.
  ASSERT_TRUE(SetCookie(
      GURL("https://c.com"), "A=0; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://d.com"))));
  ASSERT_TRUE(SetCookie(
      GURL("https://c.com"), "A=0; secure; samesite=none; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("http://e.com"))));
  ASSERT_TRUE(SetCookie(
      GURL("https://f.com"),
      "B=1; secure; samesite=none; max-age=10000; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://g.com"))));
  ASSERT_TRUE(
      SetCookie(GURL("https://f.com"), "C=2; secure; samesite=none",
                net::CookiePartitionKey::FromURLForTesting(
                    GURL("https://g.com"), base::UnguessableToken::Create())));

  ASSERT_EQ(7u, GetAllCookies().size());
  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES);
  EXPECT_EQ(0u, GetAllCookies().size());
}

IN_PROC_BROWSER_TEST_F(CookiesBrowsingDataRemoverImplBrowserTest,
                       ClearingCookiesByHostKey) {
  // Cookies set by a.com, should be removed.
  // partition_key: null, host_key: a.com
  ASSERT_TRUE(SetCookie(GURL("https://a.com"), "A=0; secure; partitioned",
                        /*cookie_partition_key=*/absl::nullopt));
  // partition_key: a.com, host_key: a.com
  ASSERT_TRUE(SetCookie(
      GURL("https://a.com"), "B=1; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://a.com"))));
  // partition_key: b.com, host_key: a.com
  ASSERT_TRUE(SetCookie(
      GURL("https://a.com"), "C=2; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://b.com"))));

  // Cookies set by b.com, should not be removed.
  // partition_key: null, host_key: b.com
  ASSERT_TRUE(SetCookie(GURL("https://b.com"), "D=3; secure; partitioned",
                        /*cookie_partition_key=*/absl::nullopt));
  // partition_key: a.com, host_key: b.com
  ASSERT_TRUE(SetCookie(
      GURL("https://b.com"), "E=4; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://a.com"))));
  // partition_key: b.com, host_key: b.com
  ASSERT_TRUE(SetCookie(
      GURL("https://b.com"), "F=5; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://b.com"))));

  ASSERT_EQ(6u, GetAllCookies().size());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain("a.com");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES,
                          std::move(builder));
  auto cookies = GetAllCookies();
  EXPECT_EQ(3u, cookies.size());
  EXPECT_EQ("D", cookies[0].Name());
  EXPECT_EQ("E", cookies[1].Name());
  EXPECT_EQ("F", cookies[2].Name());
}

IN_PROC_BROWSER_TEST_F(CookiesBrowsingDataRemoverImplBrowserTest,
                       ClearSiteData_PartitionedStateAllowedOnly) {
  // Unpartitioned cookie should not be removed when third-party cookie blocking
  // applies to the request that sent Clear-Site-Data.
  ASSERT_TRUE(SetCookie(GURL("https://a.com"), "A=0; secure;",
                        /*cookie_partition_key=*/absl::nullopt));
  // Partitioned cookies should still be removed.
  ASSERT_TRUE(SetCookie(
      GURL("https://a.com"), "B=1; secure; partitioned",
      net::CookiePartitionKey::FromURLForTesting(GURL("https://b.com"))));
  // Nonced partitioned cookies should still be removed.
  ASSERT_TRUE(
      SetCookie(GURL("https://a.com"), "C=2; secure;",
                net::CookiePartitionKey::FromURLForTesting(
                    GURL("https://b.com"), base::UnguessableToken::Create())));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain("a.com");
  builder->SetPartitionedStateAllowedOnly(true);

  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_COOKIES,
                          std::move(builder));

  auto cookies = GetAllCookies();
  EXPECT_EQ(1u, cookies.size());
  EXPECT_EQ("A", cookies[0].Name());
}

namespace {
// Provide BrowsingDataRemoverImplTrustTokenTest the Trust Tokens
// feature as a mixin so that it gets set before the superclass initializes
// the test's NetworkContext, as the NetworkContext's initialization must
// occur with the feature enabled.
class WithTrustTokensEnabled {
 public:
  WithTrustTokensEnabled() {
    feature_list_.InitAndEnableFeature(network::features::kPrivateStateTokens);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests Trust Tokens clearing by calling
// TrustTokenQueryAnswerer::HasTrustTokens with a TrustTokenQueryAnswerer
// obtained from the provided NetworkContext.
//
// The Trust Tokens functionality places a cap of 2 distinct arguments to the
// |issuer| argument of
//       TrustTokenQueryAnswerer(origin)::HasTrustTokens(issuer)
// for each top-frame origin |origin|. (This limit is recorded in persistent
// storage scoped to the origin |origin| and is not related to the lifetime of
// the specific TrustTokenQueryAnswerer object.)
//
// To add an origin, the tester creates a TrustTokenQueryAnswerer parameterized
// by |origin| and calls HasTrustTokens with two distinct "priming" issuer
// arguments. This will make the Trust Tokens persistent storage record that
// |origin| is associated with each of these issuers, with the effect that
// (barring a data clear) subsequent HasTrustTokens calls with different issuer
// arguments will fail. To check if an origin is present, the tester calls
//    TrustTokenQueryAnswerer(origin)::HasTrustTokens(issuer)
// with an |issuer| argument distinct from the two earlier "priming" issuers.
// This third HasTrustTokens call will error out exactly if |origin| was
// previously added by AddOrigin.
//
// Usage:
//   >= 0 AddOrigin() - origins must be HTTPS
//   (clear data)
//   >= 0 HasOrigin()
class TrustTokensTester {
 public:
  explicit TrustTokensTester(network::mojom::NetworkContext* network_context)
      : network_context_(network_context) {}

  void AddOrigin(const url::Origin& origin) {
    mojo::Remote<network::mojom::TrustTokenQueryAnswerer> answerer;
    network_context_->GetTrustTokenQueryAnswerer(
        answerer.BindNewPipeAndPassReceiver(), origin);

    // Calling HasTrustTokens will associate the issuer argument with the
    // origin |origin|.
    //
    // Do this until the |origin| is associated with
    // network::kTrustTokenPerToplevelMaxNumberOfAssociatedIssuers many issuers
    // (namely 2; this value is not expected to change frequently).
    //
    // After the limit is reached, subsequent HasTrustToken(origin, issuer)
    // queries will fail for any issuers not in {https://prime0.example,
    // https://prime1.example} --- unless data for |origin| is cleared.
    for (int i = 0; i < 2; ++i) {
      base::RunLoop run_loop;
      answerer->HasTrustTokens(
          url::Origin::Create(
              GURL(base::StringPrintf("https://prime%d.example", i))),
          base::BindLambdaForTesting(
              [&](network::mojom::HasTrustTokensResultPtr) {
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }

  bool HasOrigin(const url::Origin& origin) {
    mojo::Remote<network::mojom::TrustTokenQueryAnswerer> answerer;
    network_context_->GetTrustTokenQueryAnswerer(
        answerer.BindNewPipeAndPassReceiver(), origin);

    base::RunLoop run_loop;
    bool has_origin = false;

    // Since https://probe.example is not among the issuer origins previously
    // provided to HasTrustTokens(origin, _) calls in AddOrigin:
    // - If data has not been cleared,
    //     HasTrustToken(origin, https://probe.example)
    //   is expected to fail with kResourceExhausted because |origin| is at
    //   its number-of-associated-issuers limit, so the answerer will refuse
    //   to answer a query for an origin it has not yet seen.
    // - If data has been cleared, the answerer should be able to fulfill the
    //   query.
    answerer->HasTrustTokens(
        url::Origin::Create(GURL("https://probe.example")),
        base::BindLambdaForTesting([&](network::mojom::HasTrustTokensResultPtr
                                           result) {
          // HasTrustTokens will error out with kResourceExhausted exactly
          // when the top-frame origin |origin| was previously added by
          // AddOrigin.
          if (result->status ==
              network::mojom::TrustTokenOperationStatus::kResourceExhausted) {
            has_origin = true;
          }

          run_loop.Quit();
        }));

    run_loop.Run();

    return has_origin;
  }

 private:
  raw_ptr<network::mojom::NetworkContext, DanglingUntriaged> network_context_;
};

}  // namespace

class BrowsingDataRemoverImplTrustTokenTest
    : public WithTrustTokensEnabled,
      public BrowsingDataRemoverImplBrowserTest {};

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest, Remove) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));

  tester.AddOrigin(origin);
  ASSERT_TRUE(tester.HasOrigin(origin));

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS);

  EXPECT_FALSE(tester.HasOrigin(origin));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest, RemoveByDomain) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddOrigin(origin);
  tester.AddOrigin(sub_origin);
  tester.AddOrigin(another_origin);

  ASSERT_TRUE(tester.HasOrigin(origin));
  ASSERT_TRUE(tester.HasOrigin(sub_origin));
  ASSERT_TRUE(tester.HasOrigin(another_origin));

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                          std::move(builder));

  EXPECT_FALSE(tester.HasOrigin(origin));
  EXPECT_FALSE(tester.HasOrigin(sub_origin));
  EXPECT_TRUE(tester.HasOrigin(another_origin));
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplTrustTokenTest,
                       PreserveByDomain) {
  TrustTokensTester tester(network_context());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddOrigin(origin);
  tester.AddOrigin(sub_origin);
  tester.AddOrigin(another_origin);
  ASSERT_TRUE(tester.HasOrigin(origin));
  ASSERT_TRUE(tester.HasOrigin(sub_origin));
  ASSERT_TRUE(tester.HasOrigin(another_origin));

  // Delete all data *except* that specified by the filter.
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_TRUST_TOKENS,
                          std::move(builder));

  EXPECT_TRUE(tester.HasOrigin(origin));
  EXPECT_TRUE(tester.HasOrigin(sub_origin));
  EXPECT_FALSE(tester.HasOrigin(another_origin));
}

class BrowsingDataRemoverImplSharedStorageBrowserTest
    : public BrowsingDataRemoverImplBrowserTest {
 public:
  BrowsingDataRemoverImplSharedStorageBrowserTest() {
    feature_list_.InitAndEnableFeature(blink::features::kSharedStorageAPI);
  }

  StoragePartition* storage_partition() {
    return shell()
        ->web_contents()
        ->GetBrowserContext()
        ->GetDefaultStoragePartition();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplSharedStorageBrowserTest,
                       Remove) {
  SharedStorageClearSiteDataTester tester(storage_partition());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));

  tester.AddConsecutiveSharedStorageEntries(origin, u"key", u"value", 10);
  EXPECT_THAT(tester.GetSharedStorageOrigins(),
              testing::UnorderedElementsAre(origin));
  EXPECT_EQ(10, tester.GetSharedStorageTotalEntries());

  RemoveAndWait(BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE);

  EXPECT_TRUE(tester.GetSharedStorageOrigins().empty());
  EXPECT_EQ(0, tester.GetSharedStorageTotalEntries());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplSharedStorageBrowserTest,
                       RemoveByDomain) {
  SharedStorageClearSiteDataTester tester(storage_partition());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddConsecutiveSharedStorageEntries(origin, u"key", u"value", 5);
  tester.AddConsecutiveSharedStorageEntries(sub_origin, u"key", u"value", 10);
  tester.AddConsecutiveSharedStorageEntries(another_origin, u"key", u"value",
                                            1);
  EXPECT_THAT(
      tester.GetSharedStorageOrigins(),
      testing::UnorderedElementsAre(origin, sub_origin, another_origin));
  EXPECT_EQ(16, tester.GetSharedStorageTotalEntries());

  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kDelete));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE,
                          std::move(builder));

  EXPECT_THAT(tester.GetSharedStorageOrigins(),
              testing::UnorderedElementsAre(another_origin));
  EXPECT_EQ(1, tester.GetSharedStorageTotalEntries());
}

IN_PROC_BROWSER_TEST_F(BrowsingDataRemoverImplSharedStorageBrowserTest,
                       PreserveByDomain) {
  SharedStorageClearSiteDataTester tester(storage_partition());

  auto origin = url::Origin::Create(GURL("https://topframe.example"));
  auto sub_origin = url::Origin::Create(GURL("https://sub.topframe.example"));
  auto another_origin =
      url::Origin::Create(GURL("https://another-topframe.example"));

  tester.AddConsecutiveSharedStorageEntries(origin, u"key", u"value", 5);
  tester.AddConsecutiveSharedStorageEntries(sub_origin, u"key", u"value", 10);
  tester.AddConsecutiveSharedStorageEntries(another_origin, u"key", u"value",
                                            1);
  EXPECT_THAT(
      tester.GetSharedStorageOrigins(),
      testing::UnorderedElementsAre(origin, sub_origin, another_origin));
  EXPECT_EQ(16, tester.GetSharedStorageTotalEntries());

  // Delete all data *except* that specified by the filter.
  std::unique_ptr<BrowsingDataFilterBuilder> builder(
      BrowsingDataFilterBuilder::Create(
          BrowsingDataFilterBuilder::Mode::kPreserve));
  builder->AddRegisterableDomain("topframe.example");
  RemoveWithFilterAndWait(BrowsingDataRemover::DATA_TYPE_SHARED_STORAGE,
                          std::move(builder));

  EXPECT_THAT(tester.GetSharedStorageOrigins(),
              testing::UnorderedElementsAre(origin, sub_origin));
  EXPECT_EQ(15, tester.GetSharedStorageTotalEntries());
}

}  // namespace content
