// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/connection_allowlist.h"

#include "base/strings/strcat.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/connection_allowlist_parser.h"
#include "services/network/public/mojom/connection_allowlist.mojom-shared.h"
#include "services/network/public/mojom/origin_or_wildcard_header_value.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {

const GURL kExampleURL = GURL("https://site.example/");
constexpr char kSerializedExampleOrigin[] = "https://site.example";

}  // namespace

class ConnectionAllowlistParserTest : public testing::Test {
 protected:
  ConnectionAllowlistParserTest() = default;

  scoped_refptr<net::HttpResponseHeaders> GetHeaders(const char* enforced,
                                                     const char* report_only) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (enforced) {
      builder.AddHeader("Connection-Allowlist", enforced);
    }
    if (report_only) {
      builder.AddHeader("Connection-Allowlist-Report-Only", report_only);
    }
    return builder.Build();
  }

  const GURL& url() { return kExampleURL; }
};

TEST_F(ConnectionAllowlistParserTest, NoHeaders) {
  auto headers = GetHeaders(nullptr, nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  EXPECT_TRUE(result.response_url.is_empty());
  EXPECT_FALSE(result.enforced);
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, EmptyHeaders) {
  auto headers = GetHeaders("", "");
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  EXPECT_EQ(url(), result.response_url);
  EXPECT_FALSE(result.enforced);
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, MalformedHeader) {
  struct {
    const char* value;
    mojom::ConnectionAllowlistIssue issue;
  } cases[] = {
      // Non-list
      {"dictionary=value", mojom::ConnectionAllowlistIssue::kInvalidHeader},

      // List with non-Inner List
      {"1", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"1.1", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"\"string\"", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"token", mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::ConnectionAllowlistIssue::kItemNotInnerList},
      {"?0", mojom::ConnectionAllowlistIssue::kItemNotInnerList},

      // Invalid allowlist type:
      {" (1)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (1.1)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (invalid-token)",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {"( :lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:)",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (?0)", mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},
      {" (\"response-origin\")",
       mojom::ConnectionAllowlistIssue::kInvalidAllowlistItemType},

      // Invalid reporting endpoint type:
      {"(); report-to=1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=1.1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=\"string\"",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=?1",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},
      {"(); report-to=?0",
       mojom::ConnectionAllowlistIssue::kReportingEndpointNotToken},

      // Invalid URL Pattern:
      {"(\"*\")", mojom::ConnectionAllowlistIssue::kInvalidUrlPattern},
      {"(\"/(\\\\d+)/\")", mojom::ConnectionAllowlistIssue::kInvalidUrlPattern},

      // Note: we're not testing dates (`@12345`) or display strings
      // (`%"display"`) because our structured field parser doesn't yet
      // support those types.
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");
    // Enforced header:
    {
      auto headers = GetHeaders(test.value, nullptr);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      EXPECT_FALSE(result.report_only);
      EXPECT_TRUE(result.enforced->allowlist.empty());
      ASSERT_EQ(1u, result.enforced->issues.size());
      EXPECT_EQ(test.issue, result.enforced->issues[0]);
      EXPECT_EQ(result.enforced->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.enforced->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }

    // Report-Only header:
    {
      auto headers = GetHeaders(nullptr, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      EXPECT_FALSE(result.enforced);
      ASSERT_TRUE(result.report_only);
      EXPECT_TRUE(result.report_only->allowlist.empty());
      ASSERT_EQ(1u, result.report_only->issues.size());
      EXPECT_EQ(test.issue, result.report_only->issues[0]);
      EXPECT_EQ(result.report_only->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.report_only->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }

    // Both headers:
    {
      auto headers = GetHeaders(test.value, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      ASSERT_TRUE(result.report_only);
      EXPECT_TRUE(result.enforced->allowlist.empty());
      EXPECT_TRUE(result.report_only->allowlist.empty());
      ASSERT_EQ(1u, result.enforced->issues.size());
      ASSERT_EQ(1u, result.report_only->issues.size());
      EXPECT_EQ(test.issue, result.enforced->issues[0]);
      EXPECT_EQ(test.issue, result.report_only->issues[0]);
      EXPECT_EQ(result.enforced->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.enforced->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
      EXPECT_EQ(result.report_only->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.report_only->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }
  }
}

TEST_F(ConnectionAllowlistParserTest, ValidAllowlists) {
  struct {
    const char* value;
    std::vector<std::string> allowlist;
  } cases[] = {
      // Empty:
      {"()", {}},

      // Single item:
      {"(\"https://site.example\")", {"https://site.example"}},
      {"(\"https://site.example/*\")", {"https://site.example/*"}},
      {"(\"https://*.site.example/*\")", {"https://*.site.example/*"}},
      {"(response-origin)", {kSerializedExampleOrigin}},

      // Multiple items:
      {"(\"https://*.site.example/\" \"https://other.example/\")",
       {"https://*.site.example/", "https://other.example/"}},
      {"(\"https://other.example/\" \"https://*.site.example/\")",
       {"https://other.example/", "https://*.site.example/"}},
      {"(response-origin \"https://other.example/\")",
       {kSerializedExampleOrigin, "https://other.example/"}},
      {"(\"https://other.example/\" response-origin)",
       {"https://other.example/", kSerializedExampleOrigin}},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.value << "`");

    // Enforced header:
    {
      auto headers = GetHeaders(test.value, nullptr);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      EXPECT_FALSE(result.report_only);

      ASSERT_EQ(0u, result.enforced->issues.size());
      EXPECT_EQ(result.enforced->allowlist, test.allowlist);
      EXPECT_EQ(result.enforced->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.enforced->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }

    // Report-Only header:
    {
      auto headers = GetHeaders(nullptr, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      EXPECT_FALSE(result.enforced);
      ASSERT_TRUE(result.report_only);

      ASSERT_EQ(0u, result.report_only->issues.size());
      EXPECT_EQ(result.report_only->allowlist, test.allowlist);
      EXPECT_EQ(result.report_only->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.report_only->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }

    // Both headers:
    {
      auto headers = GetHeaders(test.value, test.value);
      ConnectionAllowlists result =
          ParseConnectionAllowlistsFromHeaders(*headers, url());
      ASSERT_TRUE(result.enforced);
      ASSERT_TRUE(result.report_only);

      ASSERT_EQ(0u, result.enforced->issues.size());
      ASSERT_EQ(0u, result.report_only->issues.size());
      EXPECT_EQ(result.enforced->allowlist, test.allowlist);
      EXPECT_EQ(result.report_only->allowlist, test.allowlist);
      EXPECT_EQ(result.enforced->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.enforced->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
      EXPECT_EQ(result.report_only->redirect_behavior,
                ConnectionAllowlist::RedirectBehavior::kBlock);
      EXPECT_EQ(result.report_only->webrtc_behavior,
                ConnectionAllowlist::WebRtcBehavior::kBlock);
    }
  }
}

TEST_F(ConnectionAllowlistParserTest, ValidReportToEndpoint) {
  auto headers = GetHeaders("();report-to=endpoint", nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  ASSERT_TRUE(result.enforced);
  EXPECT_EQ(0u, result.enforced->issues.size());
  EXPECT_TRUE(result.enforced->allowlist.empty());
  ASSERT_TRUE(result.enforced->reporting_endpoint.has_value());
  EXPECT_EQ("endpoint", result.enforced->reporting_endpoint.value());
  EXPECT_FALSE(result.report_only);
}

TEST_F(ConnectionAllowlistParserTest, RedirectsParam) {
  struct {
    const char* header;
    ConnectionAllowlist::RedirectBehavior expected_redirects;
    ConnectionAllowlist::WebRtcBehavior expected_webrtc;
  } tests[] = {
      {"()", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();redirects", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();redirects=block", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();redirects=\"allow\"", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();redirects=allow", ConnectionAllowlist::RedirectBehavior::kAllow,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();redirects=potato", ConnectionAllowlist::RedirectBehavior::kAllow,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
  };

  for (const auto& test : tests) {
    auto headers = GetHeaders(test.header, nullptr);
    ConnectionAllowlists result =
        ParseConnectionAllowlistsFromHeaders(*headers, url());
    EXPECT_EQ(test.expected_redirects, result.enforced->redirect_behavior);
    EXPECT_EQ(test.expected_webrtc, result.enforced->webrtc_behavior);
  }
}

TEST_F(ConnectionAllowlistParserTest, WebRtcParam) {
  struct {
    const char* header;
    ConnectionAllowlist::RedirectBehavior expected_redirects;
    ConnectionAllowlist::WebRtcBehavior expected_webrtc;
  } tests[] = {
      {"()", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();webrtc", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();webrtc=block", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();webrtc=\"allow\"", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kBlock},
      {"();webrtc=allow", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kAllow},
      {"();webrtc=potato", ConnectionAllowlist::RedirectBehavior::kBlock,
       ConnectionAllowlist::WebRtcBehavior::kAllow},
  };

  for (const auto& test : tests) {
    auto headers = GetHeaders(test.header, nullptr);
    ConnectionAllowlists result =
        ParseConnectionAllowlistsFromHeaders(*headers, url());
    EXPECT_EQ(test.expected_redirects, result.enforced->redirect_behavior);
    EXPECT_EQ(test.expected_webrtc, result.enforced->webrtc_behavior);
  }
}

TEST_F(ConnectionAllowlistParserTest, MultipleLists) {
  auto headers =
      GetHeaders("(\"https://a.example\"), (\"https://b.example\")", nullptr);
  ConnectionAllowlists result =
      ParseConnectionAllowlistsFromHeaders(*headers, url());
  ASSERT_TRUE(result.enforced);
  ASSERT_EQ(1u, result.enforced->issues.size());
  EXPECT_EQ(mojom::ConnectionAllowlistIssue::kMoreThanOneList,
            result.enforced->issues[0]);
  ASSERT_EQ(1u, result.enforced->allowlist.size());
  EXPECT_EQ("https://a.example", result.enforced->allowlist[0]);
  EXPECT_FALSE(result.report_only);
  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(result.enforced.value(),
                                            GURL("https://a.example")));
  EXPECT_FALSE(ConnectionAllowlistMatchesUrl(result.enforced.value(),
                                             GURL("https://c.example")));
}

TEST_F(ConnectionAllowlistParserTest, IsAllowlisted) {
  ConnectionAllowlist connection_allowlist;
  connection_allowlist.allowlist = {"https://a.example/",
                                    "https://*.b.example/*"};

  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(connection_allowlist,
                                            GURL("https://a.example/")));
  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(
      connection_allowlist, GURL("https://sub.b.example/path")));
  EXPECT_FALSE(ConnectionAllowlistMatchesUrl(connection_allowlist,
                                             GURL("https://c.example/")));
  EXPECT_FALSE(ConnectionAllowlistMatchesUrl(connection_allowlist,
                                             GURL("http://a.example/")));

  // Local schemes are allowed, even if they're not explicitly listed:
  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(
      connection_allowlist, GURL("data:text/html,Hello world!")));
  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(
      connection_allowlist, GURL("filesystem:https://example.site/")));
  EXPECT_TRUE(ConnectionAllowlistMatchesUrl(
      connection_allowlist, GURL("blob:https://example.site/")));
}

TEST_F(ConnectionAllowlistParserTest, ParseConnectionAllowlistSingleValue) {
  // A single structured-header value parses like the `Connection-Allowlist`
  // header, resolving `response-origin` against the response URL.
  std::optional<ConnectionAllowlist> result = ParseConnectionAllowlist(
      "(\"https://other.example/\" response-origin)", url());
  ASSERT_TRUE(result);
  EXPECT_THAT(
      result->allowlist,
      testing::ElementsAre("https://other.example/", kSerializedExampleOrigin));
  EXPECT_TRUE(result->issues.empty());

  // An empty header value yields nullopt.
  EXPECT_FALSE(ParseConnectionAllowlist("", url()));

  // A malformed value yields a present allowlist carrying the issue (callers
  // decide how to treat it).
  std::optional<ConnectionAllowlist> malformed =
      ParseConnectionAllowlist("dictionary=value", url());
  ASSERT_TRUE(malformed);
  ASSERT_FALSE(malformed->issues.empty());
  EXPECT_EQ(mojom::ConnectionAllowlistIssue::kInvalidHeader,
            malformed->issues[0]);
}

TEST_F(ConnectionAllowlistParserTest,
       ParseConnectionAllowlistDeferResponseOrigin) {
  // With no `response_url` (nullopt), `response-origin` is not resolved into
  // the allowlist; instead it sets `match_response_origin` for the browser to
  // resolve later. Explicit patterns are still parsed.
  std::optional<ConnectionAllowlist> result = ParseConnectionAllowlist(
      "(\"https://other.example/\" response-origin)", std::nullopt);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->allowlist,
              testing::ElementsAre("https://other.example/"));
  EXPECT_TRUE(result->match_response_origin);
  EXPECT_TRUE(result->issues.empty());

  // Without the token, `match_response_origin` stays false when deferring.
  std::optional<ConnectionAllowlist> no_token =
      ParseConnectionAllowlist("(\"https://other.example/\")", std::nullopt);
  ASSERT_TRUE(no_token);
  EXPECT_FALSE(no_token->match_response_origin);
  EXPECT_THAT(no_token->allowlist,
              testing::ElementsAre("https://other.example/"));

  // The default (resolving) path resolves the token into the allowlist and
  // leaves `match_response_origin` false.
  std::optional<ConnectionAllowlist> resolved =
      ParseConnectionAllowlist("(response-origin)", url());
  ASSERT_TRUE(resolved);
  EXPECT_FALSE(resolved->match_response_origin);
  EXPECT_THAT(resolved->allowlist,
              testing::ElementsAre(kSerializedExampleOrigin));
}

TEST_F(ConnectionAllowlistParserTest, Subsumes) {
  ConnectionAllowlist required;
  required.allowlist = {"https://a.example/", "https://b.example/"};

  // A candidate whose endpoints are a subset is at least as strict.
  ConnectionAllowlist subset;
  subset.allowlist = {"https://a.example/"};
  EXPECT_TRUE(ConnectionAllowlistSubsumes(required, subset));

  // An equal candidate is subsumed.
  EXPECT_TRUE(ConnectionAllowlistSubsumes(required, required));

  // An empty candidate is trivially subsumed (it is maximally strict).
  ConnectionAllowlist empty;
  EXPECT_TRUE(ConnectionAllowlistSubsumes(required, empty));

  // A candidate permitting an endpoint not in `required` is not subsumed.
  ConnectionAllowlist superset;
  superset.allowlist = {"https://a.example/", "https://c.example/"};
  EXPECT_FALSE(ConnectionAllowlistSubsumes(required, superset));

  // Comparison is syntactic: a semantically-similar but differently-spelled
  // pattern is not considered subsumed.
  ConnectionAllowlist different_spelling;
  different_spelling.allowlist = {"https://a.example"};  // no trailing slash
  EXPECT_FALSE(ConnectionAllowlistSubsumes(required, different_spelling));
}

TEST_F(ConnectionAllowlistParserTest, SubsumesRedirectAndWebRtcStrictness) {
  ConnectionAllowlist required;  // redirect/webrtc default to kBlock.
  ASSERT_EQ(required.redirect_behavior,
            ConnectionAllowlist::RedirectBehavior::kBlock);
  ASSERT_EQ(required.webrtc_behavior,
            ConnectionAllowlist::WebRtcBehavior::kBlock);

  // A candidate that blocks both is at least as strict.
  ConnectionAllowlist strict;
  EXPECT_TRUE(ConnectionAllowlistSubsumes(required, strict));

  // A candidate that allows redirects when `required` blocks is not subsumed.
  ConnectionAllowlist allows_redirects;
  allows_redirects.redirect_behavior =
      ConnectionAllowlist::RedirectBehavior::kAllow;
  EXPECT_FALSE(ConnectionAllowlistSubsumes(required, allows_redirects));

  // A candidate that allows WebRTC when `required` blocks is not subsumed.
  ConnectionAllowlist allows_webrtc;
  allows_webrtc.webrtc_behavior = ConnectionAllowlist::WebRtcBehavior::kAllow;
  EXPECT_FALSE(ConnectionAllowlistSubsumes(required, allows_webrtc));

  // When `required` itself allows redirects/WebRTC, a candidate that allows
  // them is still subsumed (the candidate need not be stricter than required).
  ConnectionAllowlist lenient;
  lenient.redirect_behavior = ConnectionAllowlist::RedirectBehavior::kAllow;
  lenient.webrtc_behavior = ConnectionAllowlist::WebRtcBehavior::kAllow;
  EXPECT_TRUE(ConnectionAllowlistSubsumes(lenient, allows_redirects));
  EXPECT_TRUE(ConnectionAllowlistSubsumes(lenient, strict));
}

TEST_F(ConnectionAllowlistParserTest, ParseAllowConnectionAllowlistFrom) {
  auto build = [](const char* value) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (value) {
      builder.AddHeader("Allow-Connection-Allowlist-From", value);
    }
    return builder.Build();
  };

  // Absent header -> null.
  EXPECT_FALSE(ParseAllowConnectionAllowlistFromHeader(*build(nullptr)));

  // `*` -> allow-star.
  {
    mojom::OriginOrWildcardHeaderValuePtr result =
        ParseAllowConnectionAllowlistFromHeader(*build("*"));
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_allow_star());
  }

  // A valid origin -> origin.
  {
    mojom::OriginOrWildcardHeaderValuePtr result =
        ParseAllowConnectionAllowlistFromHeader(
            *build("https://embedder.example"));
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->is_origin());
    EXPECT_EQ(result->get_origin(),
              url::Origin::Create(GURL("https://embedder.example")));
  }

  // An invalid value -> error message.
  {
    mojom::OriginOrWildcardHeaderValuePtr result =
        ParseAllowConnectionAllowlistFromHeader(*build("not a url"));
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_error_message());
  }

  // Values that parse as URLs but aren't bare origin serializations (a trailing
  // slash, a path, or userinfo) are rejected -- only an exact origin is valid.
  for (const char* invalid :
       {"https://embedder.example/", "https://embedder.example/path",
        "https://user@embedder.example"}) {
    mojom::OriginOrWildcardHeaderValuePtr result =
        ParseAllowConnectionAllowlistFromHeader(*build(invalid));
    ASSERT_TRUE(result) << invalid;
    EXPECT_TRUE(result->is_error_message()) << invalid;
  }
}

TEST_F(ConnectionAllowlistParserTest, AllowsBlanketEnforcement) {
  const url::Origin embedder =
      url::Origin::Create(GURL("https://embedder.example"));
  const GURL network_url("https://widget.example/");

  // Local schemes always allow blanket enforcement, regardless of header.
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("about:srcdoc"), nullptr));
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("data:text/html,hi"), nullptr));
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("blob:https://x.example/abc"), nullptr));
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("filesystem:https://x.example/temporary/a"), nullptr));

  // The scheme is checked before any opt-in header, so a local scheme is
  // honored even when a non-matching header is present (blobs may grow headers
  // in the future).
  mojom::OriginOrWildcardHeaderValuePtr non_matching =
      mojom::OriginOrWildcardHeaderValue::NewOrigin(
          url::Origin::Create(GURL("https://evil.example")));
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("blob:https://x.example/abc"), non_matching.get()));

  // `file:` is not a local scheme (matching Fetch), so it does not get blanket
  // enforcement.
  EXPECT_FALSE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, GURL("file:///etc/passwd"), nullptr));

  // A network-scheme response with no opt-in header does not allow it.
  EXPECT_FALSE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, network_url, nullptr));

  // `*` allows any embedder.
  mojom::OriginOrWildcardHeaderValuePtr star =
      mojom::OriginOrWildcardHeaderValue::NewAllowStar(true);
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, network_url, star.get()));

  // A matching origin allows that embedder.
  mojom::OriginOrWildcardHeaderValuePtr matching =
      mojom::OriginOrWildcardHeaderValue::NewOrigin(embedder);
  EXPECT_TRUE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, network_url, matching.get()));

  // A non-matching origin does not.
  mojom::OriginOrWildcardHeaderValuePtr other =
      mojom::OriginOrWildcardHeaderValue::NewOrigin(
          url::Origin::Create(GURL("https://evil.example")));
  EXPECT_FALSE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, network_url, other.get()));

  // An error-message value does not.
  mojom::OriginOrWildcardHeaderValuePtr error =
      mojom::OriginOrWildcardHeaderValue::NewErrorMessage("bad");
  EXPECT_FALSE(AllowsBlanketEnforcementOfRequiredConnectionAllowlist(
      embedder, network_url, error.get()));
}

namespace {

void FuzzConnectionAllowlistParser(const std::string& enforced,
                                   const std::string& report_only) {
  std::string raw_headers = base::StrCat(
      {"HTTP/1.1 200 OK\n", "Connection-Allowlist: ", enforced, "\n",
       "Connection-Allowlist-Report-Only: ", report_only, "\n\n"});
  auto headers = net::HttpResponseHeaders::TryToCreate(raw_headers);
  if (!headers) {
    return;
  }

  std::ignore = network::ParseConnectionAllowlistsFromHeaders(
      *headers, GURL("https://site.example/"));
}

FUZZ_TEST(ConnectionAllowlistFuzz, FuzzConnectionAllowlistParser);

}  // namespace

}  // namespace network
