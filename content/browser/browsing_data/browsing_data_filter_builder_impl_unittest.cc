// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/browsing_data/browsing_data_filter_builder_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/scoped_feature_list.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_deletion_info.h"
#include "services/network/cookie_manager.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using CookieDeletionInfo = net::CookieDeletionInfo;

using testing::IsEmpty;
using testing::UnorderedElementsAre;

const char kGoogleDomain[] = "google.com";
// sp.nom.br is an eTLD, so this is a regular valid registrable domain, just
// like google.com.
const char kLongETLDDomain[] = "website.sp.nom.br";
// This domain will also not be found in registries, and since it has only
// one component, it will not be recognized as a valid registrable domain.
const char kInternalHostname[] = "fileserver";
// This domain will not be found in registries. It will be assumed that
// it belongs to an unknown registry, and since it has two components,
// they will be treated as the second level domain and TLD. Most importantly,
// it will NOT be treated as a subdomain of "fileserver".
const char kUnknownRegistryDomain[] = "second-level-domain.fileserver";
// IP addresses are supported.
const char kIPAddress[] = "192.168.1.1";

struct TestCase {
  std::string url;
  bool should_match;
};

void RunTestCase(TestCase test_case,
                 const base::RepeatingCallback<bool(const GURL&)>& filter) {
  GURL url(test_case.url);
  EXPECT_TRUE(url.is_valid()) << test_case.url << " is not valid.";
  EXPECT_EQ(test_case.should_match, filter.Run(GURL(test_case.url)))
      << test_case.url;
}

void RunTestCase(TestCase test_case,
                 network::mojom::CookieDeletionFilterPtr deletion_filter) {
  // Test with regular cookie, http only, domain, and secure.
  CookieDeletionInfo delete_info =
      network::DeletionFilterToInfo(std::move(deletion_filter));
  std::string cookie_line = "A=2";
  GURL test_url(test_case.url);
  EXPECT_TRUE(test_url.is_valid()) << test_case.url;
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  EXPECT_TRUE(cookie) << cookie_line << " from " << test_case.url
                      << " is not a valid cookie";
  if (cookie) {
    EXPECT_EQ(test_case.should_match,
              delete_info.Matches(
                  *cookie,
                  net::CookieAccessParams{
                      net::CookieAccessSemantics::NONLEGACY, false,
                      net::CookieSamePartyStatus::kNoSamePartyEnforcement}))
        << cookie->DebugString();
  }

  cookie_line = std::string("A=2;domain=") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  if (cookie) {
    EXPECT_EQ(test_case.should_match,
              delete_info.Matches(
                  *cookie,
                  net::CookieAccessParams{
                      net::CookieAccessSemantics::NONLEGACY, false,
                      net::CookieSamePartyStatus::kNoSamePartyEnforcement}))
        << cookie->DebugString();
  }

  cookie_line = std::string("A=2; HttpOnly;") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  if (cookie) {
    EXPECT_EQ(test_case.should_match,
              delete_info.Matches(
                  *cookie,
                  net::CookieAccessParams{
                      net::CookieAccessSemantics::NONLEGACY, false,
                      net::CookieSamePartyStatus::kNoSamePartyEnforcement}))
        << cookie->DebugString();
  }

  cookie_line = std::string("A=2; HttpOnly; Secure;") + test_url.host();
  cookie = net::CanonicalCookie::Create(
      test_url, cookie_line, base::Time::Now(), absl::nullopt /* server_time */,
      absl::nullopt /* cookie_partition_key */);
  if (cookie) {
    EXPECT_EQ(test_case.should_match,
              delete_info.Matches(
                  *cookie,
                  net::CookieAccessParams{
                      net::CookieAccessSemantics::NONLEGACY, false,
                      net::CookieSamePartyStatus::kNoSamePartyEnforcement}))
        << cookie->DebugString();
  }
}

void RunTestCase(
    TestCase test_case,
    const base::RepeatingCallback<bool(const std::string&)>& filter) {
  std::string channel_id_server_id = test_case.url;
  EXPECT_EQ(test_case.should_match, filter.Run(channel_id_server_id))
      << channel_id_server_id << " should "
      << (test_case.should_match ? "" : "NOT ") << "be matched by the filter.";
}

struct StorageKeyTestCase {
  std::string origin;
  std::string top_level_site;
  blink::mojom::AncestorChainBit ancestor_chain_bit;
  bool should_match;
};

void RunTestCase(
    StorageKeyTestCase test_case,
    const content::StoragePartition::StorageKeyMatcherFunction& filter) {
  auto origin = url::Origin::Create(GURL(test_case.origin));
  auto top_level_site =
      net::SchemefulSite(url::Origin::Create(GURL(test_case.top_level_site)));
  auto key = blink::StorageKey::Create(origin, top_level_site,
                                       test_case.ancestor_chain_bit);
  EXPECT_EQ(test_case.should_match, filter.Run(key))
      << key.GetDebugString() << " should "
      << (test_case.should_match ? "" : "NOT ") << "be matched by the filter.";
}

}  // namespace

TEST(BrowsingDataFilterBuilderImplTest, Noop) {
  // An no-op filter matches everything.
  base::RepeatingCallback<bool(const GURL&)> filter =
      BrowsingDataFilterBuilder::BuildNoopFilter();

  TestCase test_cases[] = {
      {"https://www.google.com", true},
      {"https://www.chrome.com", true},
      {"http://www.google.com/foo/bar", true},
      {"https://website.sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, RegistrableDomainGURLDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // We match any URL on the specified domains.
      {"http://www.google.com/foo/bar", true},
      {"https://www.sub.google.com/foo/bar", true},
      {"https://sub.google.com", true},
      {"http://www.sub.google.com:8000/foo/bar", true},
      {"https://website.sp.nom.br", true},
      {"https://www.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:80", true},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver/foo/bar", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", true},
      {"http://www.second-level-domain.fileserver/index.html", true},

      // Different domains.
      {"https://www.youtube.com", false},
      {"https://www.google.net", false},
      {"http://192.168.1.2", false},

      // Check both a bare eTLD.
      {"https://sp.nom.br", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, RegistrableDomainGURLPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // We match any URL that are not on the specified domains.
      {"http://www.google.com/foo/bar", false},
      {"https://www.sub.google.com/foo/bar", false},
      {"https://sub.google.com", false},
      {"http://www.sub.google.com:8000/foo/bar", false},
      {"https://website.sp.nom.br", false},
      {"https://www.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:80", false},

      // Internal hostnames do not have subdomains.
      {"http://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver/foo/bar", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver/foo", false},
      {"http://www.second-level-domain.fileserver/index.html", false},

      // Different domains.
      {"https://www.youtube.com", true},
      {"https://www.google.net", true},
      {"http://192.168.1.2", true},

      // Check our bare eTLD.
      {"https://sp.nom.br", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesCookiesDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));

  TestCase test_cases[] = {
      // Any cookie with the same registerable domain as the origins is matched.
      {"https://www.google.com", true},
      {"http://www.google.com", true},
      {"http://www.google.com:300", true},
      {"https://mail.google.com", true},
      {"http://mail.google.com", true},
      {"http://google.com", true},
      {"https://website.sp.nom.br", true},
      {"https://sub.website.sp.nom.br", true},
      {"http://192.168.1.1", true},
      {"http://192.168.1.1:10", true},

      // Different eTLDs.
      {"https://www.google.org", false},
      {"https://www.google.co.uk", false},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", false},
      {"https://sp.nom.br", false},

      // Different hosts in general.
      {"https://www.chrome.com", false},
      {"http://192.168.2.1", false},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", true },
      {"http://fileserver/foo/bar", true },
      {"http://website.fileserver", false },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", true},
      {"https://subdomain.second-level-domain.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, builder.BuildCookieDeletionFilter());
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesCookiesPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));

  TestCase test_cases[] = {
      // Any cookie that doesn't have the same registerable domain is matched.
      {"https://www.google.com", false},
      {"http://www.google.com", false},
      {"http://www.google.com:300", false},
      {"https://mail.google.com", false},
      {"http://mail.google.com", false},
      {"http://google.com", false},
      {"https://website.sp.nom.br", false},
      {"https://sub.website.sp.nom.br", false},
      {"http://192.168.1.1", false},
      {"http://192.168.1.1:10", false},

      // Different eTLDs.
      {"https://www.google.org", true},
      {"https://www.google.co.uk", true},

      // We treat eTLD+1 and bare eTLDs as different domains.
      {"https://www.sp.nom.br", true},
      {"https://sp.nom.br", true},

      // Different hosts in general.
      {"https://www.chrome.com", true},
      {"http://192.168.2.1", true},

      // Internal hostnames do not have subdomains.
      {"https://fileserver", false },
      {"http://fileserver/foo/bar", false },
      {"http://website.fileserver", true },

      // This is a valid registrable domain with the TLD "fileserver", which
      // is unrelated to the internal hostname "fileserver".
      {"http://second-level-domain.fileserver", false},
      {"https://subdomain.second-level-domain.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, builder.BuildCookieDeletionFilter());
}

TEST(BrowsingDataFilterBuilderImplTest, PartitionedCookies) {
  struct PartitionedCookiesTestCase {
    net::CookiePartitionKeyCollection filter_cookie_partition_key_collection;
    absl::optional<net::CookiePartitionKey> cookie_partition_key;
    bool should_match;
  } test_cases[] = {
      // Unpartitioned cookies should remain unaffected by the filter's
      // keychain.
      {net::CookiePartitionKeyCollection(), absl::nullopt, true},
      {net::CookiePartitionKeyCollection::ContainsAll(), absl::nullopt, true},
      {net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://www.foo.com"))),
       absl::nullopt, true},
      // Partitioned cookies should not match with an empty keychain.
      {net::CookiePartitionKeyCollection(),
       net::CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
       false},
      // Partitioned cookies should match a keychain with their partition key.
      {net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://www.foo.com"))),
       net::CookiePartitionKey::FromURLForTesting(
           GURL("https://subdomain.foo.com")),
       true},
      // Partitioned cookies should match a keychain that contains all keys.
      {net::CookiePartitionKeyCollection::ContainsAll(),
       net::CookiePartitionKey::FromURLForTesting(GURL("https://www.foo.com")),
       true},
      // Partitioned cookies should not match a keychain with a different
      // partition key.
      {net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://www.foo.com"))),
       net::CookiePartitionKey::FromURLForTesting(GURL("https://www.bar.com")),
       false},
  };

  for (const auto& test_case : test_cases) {
    BrowsingDataFilterBuilderImpl builder(
        BrowsingDataFilterBuilderImpl::Mode::kDelete);
    builder.AddRegisterableDomain("cookie.com");
    builder.SetCookiePartitionKeyCollection(
        test_case.filter_cookie_partition_key_collection);

    CookieDeletionInfo delete_info =
        network::DeletionFilterToInfo(builder.BuildCookieDeletionFilter());
    std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
        GURL("https://www.cookie.com/"),
        "__Host-A=B; Secure; SameSite=None; Path=/; Partitioned;",
        base::Time::Now(), absl::nullopt, test_case.cookie_partition_key);
    EXPECT_TRUE(cookie);
    EXPECT_EQ(
        test_case.should_match,
        delete_info.Matches(
            *cookie, net::CookieAccessParams{
                         net::CookieAccessSemantics::NONLEGACY, false,
                         net::CookieSamePartyStatus::kNoSamePartyEnforcement}));
  }
}

TEST(BrowsingDataFilterBuilderImplTest, IsCrossSiteClearSiteDataForCookies) {
  struct TestCase {
    const std::string desc;
    const net::CookiePartitionKeyCollection cookie_partition_key_collection;
    bool expected;
  } test_cases[] = {
      {"Empty keychain", net::CookiePartitionKeyCollection(), false},
      {"Keychain contains all keys",
       net::CookiePartitionKeyCollection::ContainsAll(), false},
      {"Contains secure cookie domain",
       net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("http://cookie.com"))),
       false},
      {"Contains insecure cookie domain",
       net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://cookie.com"))),
       false},
      {"Does not include cookie domain (secure)",
       net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://notcookie.com"))),
       true},
      {"Does not include cookie domain (insecure)",
       net::CookiePartitionKeyCollection(
           net::CookiePartitionKey::FromURLForTesting(
               GURL("http://notcookie.com"))),
       true},
      {"Multiple keys, contains cookie domain",
       net::CookiePartitionKeyCollection({
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://cookie.com")),
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://notcookie.com")),
       }),
       false},
      {"Multiple keys, does not contain cookie domain",
       net::CookiePartitionKeyCollection({
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://notcookie.com")),
           net::CookiePartitionKey::FromURLForTesting(
               GURL("https://alsonotcookie.com")),
       }),
       true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.desc);
    BrowsingDataFilterBuilderImpl builder(
        BrowsingDataFilterBuilderImpl::Mode::kDelete);
    builder.AddRegisterableDomain("cookie.com");
    builder.SetCookiePartitionKeyCollection(
        test_case.cookie_partition_key_collection);
    EXPECT_EQ(test_case.expected, builder.IsCrossSiteClearSiteDataForCookies());
  }
}

TEST(BrowsingDataFilterBuilderImplTest, StorageKey) {
  base::test::ScopedFeatureList scope_feature_list;
  scope_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);
  auto origin1 = url::Origin::Create(GURL("https://foo.com"));
  auto origin2 = url::Origin::Create(GURL("https://bar.com"));
  absl::optional<blink::StorageKey> keys[] = {
      // No storage key provided.
      absl::nullopt,
      // Top-level (Foo).
      blink::StorageKey::CreateFromStringForTesting("https://foo.com"),
      // Foo -> Bar.
      blink::StorageKey::Create(origin1, net::SchemefulSite(origin2),
                                blink::mojom::AncestorChainBit::kCrossSite),
      // Foo -> Bar -> Foo.
      blink::StorageKey::Create(origin1, net::SchemefulSite(origin1),
                                blink::mojom::AncestorChainBit::kCrossSite),
      // Bar
      blink::StorageKey::Create(origin2, net::SchemefulSite(origin2),
                                blink::mojom::AncestorChainBit::kSameSite),
      // Bar -> Foo
      blink::StorageKey::Create(origin2, net::SchemefulSite(origin1),
                                blink::mojom::AncestorChainBit::kCrossSite),
  };

  // Test for OriginMatchingMode::kThirdPartiesIncluded.
  for (size_t i = 0; i < std::size(keys); ++i) {
    const auto& storage_key = keys[i];
    BrowsingDataFilterBuilderImpl builder(
        BrowsingDataFilterBuilderImpl::Mode::kDelete,
        BrowsingDataFilterBuilderImpl::OriginMatchingMode::
            kThirdPartiesIncluded);
    builder.AddOrigin((storage_key.has_value()) ? storage_key.value().origin()
                                                : origin1);
    builder.SetStorageKey(storage_key);
    EXPECT_EQ(storage_key.has_value(), builder.HasStorageKey());
    // Start from 1 to ignore the nullopt key.
    for (size_t j = 1; j < std::size(keys); ++j) {
      const auto& key_to_compare = keys[j];
      auto matcher_function = builder.BuildStorageKeyFilter();
      // Only matches either when the keys are exactly the same, or when there
      // is no stored key and the origin matching is performed.
      bool same_key = (i == j);
      bool origin_match =
          (!storage_key.has_value() && key_to_compare.has_value() &&
           key_to_compare.value().MatchesOriginForTrustedStorageDeletion(
               origin1));
      bool expected = same_key || origin_match;
      EXPECT_EQ(expected,
                std::move(matcher_function).Run(key_to_compare.value()));
    }
  }

  // Test for OriginMatchingMode::kOriginInAllContexts
  for (size_t i = 0; i < std::size(keys); ++i) {
    const auto& storage_key = keys[i];
    BrowsingDataFilterBuilderImpl builder(
        BrowsingDataFilterBuilderImpl::Mode::kDelete,
        BrowsingDataFilterBuilderImpl::OriginMatchingMode::
            kOriginInAllContexts);
    builder.AddOrigin((storage_key.has_value()) ? storage_key.value().origin()
                                                : origin1);
    builder.SetStorageKey(storage_key);
    EXPECT_EQ(storage_key.has_value(), builder.HasStorageKey());
    // Start from 1 to ignore the nullopt key.
    for (size_t j = 1; j < std::size(keys); ++j) {
      const auto& key_to_compare = keys[j];
      auto matcher_function = builder.BuildStorageKeyFilter();
      // Only matches either when the keys are exactly the same, or when there
      // is no stored key and the origin is the same.
      bool same_key = (i == j);
      bool same_origin =
          (!storage_key.has_value() && key_to_compare.has_value() &&
           key_to_compare.value().origin() == origin1);
      bool expected = same_key || same_origin;
      EXPECT_EQ(expected,
                std::move(matcher_function).Run(key_to_compare.value()));
    }
  }
}

TEST(BrowsingDataFilterBuilderImplTest, NetworkServiceFilterDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  ASSERT_EQ(BrowsingDataFilterBuilderImpl::Mode::kDelete, builder.GetMode());
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  network::mojom::ClearDataFilterPtr filter =
      builder.BuildNetworkServiceFilter();

  EXPECT_EQ(network::mojom::ClearDataFilter_Type::DELETE_MATCHES, filter->type);
  EXPECT_THAT(filter->domains, testing::UnorderedElementsAre(
                                   kGoogleDomain, kLongETLDDomain, kIPAddress,
                                   kUnknownRegistryDomain, kInternalHostname));
  EXPECT_TRUE(filter->origins.empty());
}

TEST(BrowsingDataFilterBuilderImplTest, NetworkServiceFilterPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  ASSERT_EQ(BrowsingDataFilterBuilderImpl::Mode::kPreserve, builder.GetMode());
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  network::mojom::ClearDataFilterPtr filter =
      builder.BuildNetworkServiceFilter();

  EXPECT_EQ(network::mojom::ClearDataFilter_Type::KEEP_MATCHES, filter->type);
  EXPECT_THAT(filter->domains, testing::UnorderedElementsAre(
                                   kGoogleDomain, kLongETLDDomain, kIPAddress,
                                   kUnknownRegistryDomain, kInternalHostname));
  EXPECT_TRUE(filter->origins.empty());
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesPluginSitesDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const std::string&)> filter =
      builder.BuildPluginFilter();

  TestCase test_cases[] = {
      // Plugin sites can be domains, ...
      {"google.com", true},
      {"www.google.com", true},
      {"website.sp.nom.br", true},
      {"www.website.sp.nom.br", true},
      {"second-level-domain.fileserver", true},
      {"foo.bar.second-level-domain.fileserver", true},

      // ... IP addresses, or internal hostnames.
      {"192.168.1.1", true},
      {"fileserver", true},

      // Sites not added to the filter are not matched.
      {"example.com", false},
      {"192.168.1.2", false},
      {"website.fileserver", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest,
     RegistrableDomainMatchesPluginSitesPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  builder.AddRegisterableDomain(std::string(kIPAddress));
  builder.AddRegisterableDomain(std::string(kUnknownRegistryDomain));
  builder.AddRegisterableDomain(std::string(kInternalHostname));
  base::RepeatingCallback<bool(const std::string&)> filter =
      builder.BuildPluginFilter();

  TestCase test_cases[] = {
      // Plugin sites can be domains, ...
      {"google.com", false},
      {"www.google.com", false},
      {"website.sp.nom.br", false},
      {"www.website.sp.nom.br", false},
      {"second-level-domain.fileserver", false},
      {"foo.bar.second-level-domain.fileserver", false},

      // ... IP addresses, or internal hostnames.
      {"192.168.1.1", false},
      {"fileserver", false},

      // Sites not added to the list of origins to preserve are matched.
      {"example.com", true},
      {"192.168.1.2", true},
      {"website.fileserver", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, OriginDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddOrigin(url::Origin::Create(GURL("https://www.google.com")));
  builder.AddOrigin(url::Origin::Create(GURL("http://www.example.com")));
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // A kDelete filter matches any URL on the specified origins.
      {"https://www.google.com", true},
      {"https://www.google.com/?q=test", true},
      {"http://www.example.com", true},
      {"http://www.example.com/index.html", true},
      {"http://www.example.com/foo/bar", true},

      // Subdomains are different origins.
      {"https://test.www.google.com", false},

      // Different scheme or port is a different origin.
      {"https://www.google.com:8000", false},
      {"https://www.example.com/index.html", false},

      // Different host is a different origin.
      {"https://www.youtube.com", false},
      {"https://www.chromium.org", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, OriginPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddOrigin(url::Origin::Create(GURL("https://www.google.com")));
  builder.AddOrigin(url::Origin::Create(GURL("http://www.example.com")));
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // URLS on explicitly specified origins are not matched.
      {"https://www.google.com", false},
      {"https://www.google.com/?q=test", false},
      {"http://www.example.com", false},
      {"http://www.example.com/index.html", false},
      {"http://www.example.com/foo/bar", false},

      // Subdomains are different origins.
      {"https://test.www.google.com", true},

      // The same hosts but with different schemes and ports are not preserved.
      {"https://www.google.com:8000", true},
      {"https://www.example.com/index.html", true},

      // Different hosts are not preserved.
      {"https://www.chrome.com", true},
      {"https://www.youtube.com", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, CombinedDeleteList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddOrigin(url::Origin::Create(GURL("https://google.com")));
  builder.AddRegisterableDomain("example.com");
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // Deletelist matches any URL on the specified origins.
      {"https://google.com/foo/bar", true},
      {"https://example.com/?q=test", true},

      // Since www.google.com was added as an origin, its subdomains are not
      // matched. However, example.com was added as a registrable domain,
      // so its subdomains are matched.
      {"https://www.google.com/foo/bar", false},
      {"https://www.example.com/?q=test", true},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, CombinedPreserveList) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddOrigin(url::Origin::Create(GURL("https://google.com")));
  builder.AddRegisterableDomain("example.com");
  base::RepeatingCallback<bool(const GURL&)> filter = builder.BuildUrlFilter();

  TestCase test_cases[] = {
      // URLS on explicitly specified origins are not matched.
      {"https://google.com/foo/bar", false},
      {"https://example.com/?q=test", false},

      // Since www.google.com was added as an origin, its subdomains are not
      // preserved. However, example.com was added as a registrable domain, so
      // its subdomains are also preserved.
      {"https://www.google.com/foo/bar", true},
      {"https://www.example.com/?q=test", false},
  };

  for (TestCase test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, PartitionedDeleteList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const std::string origin1 = "https://www.google.com";
  const std::string origin2 = "https://example.com";
  const std::string origin3 = "https://maps.google.com";

  ASSERT_TRUE(blink::StorageKey::IsThirdPartyStoragePartitioningEnabled());
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);
  builder.AddOrigin(url::Origin::Create(GURL(origin1)));
  auto filter = builder.BuildStorageKeyFilter();

  StorageKeyTestCase test_cases[] = {
      // Top-level sites with origin1.
      {origin1, origin1, blink::mojom::AncestorChainBit::kSameSite, true},
      {origin2, origin1, blink::mojom::AncestorChainBit::kCrossSite, true},
      {origin1, origin1, blink::mojom::AncestorChainBit::kCrossSite, true},
      // Top-level sites with origin2.
      {origin2, origin2, blink::mojom::AncestorChainBit::kSameSite, false},
      {origin1, origin2, blink::mojom::AncestorChainBit::kCrossSite, false},
      // Same top-level domain as origin1.
      {origin3, origin3, blink::mojom::AncestorChainBit::kSameSite, false},
      {origin2, origin3, blink::mojom::AncestorChainBit::kCrossSite, true},
      {origin3, origin3, blink::mojom::AncestorChainBit::kCrossSite, true},
  };

  for (auto test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, PartitionedPreserveList) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      net::features::kThirdPartyStoragePartitioning);

  const std::string origin1 = "https://www.google.com";
  const std::string origin2 = "https://example.com";
  const std::string origin3 = "https://maps.google.com";

  ASSERT_TRUE(blink::StorageKey::IsThirdPartyStoragePartitioningEnabled());
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kPreserve);
  builder.AddOrigin(url::Origin::Create(GURL(origin1)));
  auto filter = builder.BuildStorageKeyFilter();

  StorageKeyTestCase test_cases[] = {
      // Top-level sites with origin1.
      {origin1, origin1, blink::mojom::AncestorChainBit::kSameSite, false},
      {origin2, origin1, blink::mojom::AncestorChainBit::kCrossSite, false},
      {origin1, origin1, blink::mojom::AncestorChainBit::kCrossSite, false},
      // Top-level sites with origin2.
      {origin2, origin2, blink::mojom::AncestorChainBit::kSameSite, true},
      {origin1, origin2, blink::mojom::AncestorChainBit::kCrossSite, true},
      // Same top-level domain as origin1.
      {origin3, origin3, blink::mojom::AncestorChainBit::kSameSite, true},
      {origin2, origin3, blink::mojom::AncestorChainBit::kCrossSite, false},
      {origin3, origin3, blink::mojom::AncestorChainBit::kCrossSite, false},
  };

  for (auto test_case : test_cases)
    RunTestCase(test_case, filter);
}

TEST(BrowsingDataFilterBuilderImplTest, GetOrigins) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);

  url::Origin a = url::Origin::Create(GURL("https://www.google.com"));
  url::Origin b = url::Origin::Create(GURL("http://www.example.com"));
  EXPECT_THAT(builder.GetOrigins(), IsEmpty());
  builder.AddOrigin(a);
  EXPECT_THAT(builder.GetOrigins(), UnorderedElementsAre(a));
  builder.AddOrigin(b);
  EXPECT_THAT(builder.GetOrigins(), UnorderedElementsAre(a, b));
}

TEST(BrowsingDataFilterBuilderImplTest, GetRegisterableDomains) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);

  EXPECT_THAT(builder.GetRegisterableDomains(), IsEmpty());
  builder.AddRegisterableDomain(std::string(kGoogleDomain));
  EXPECT_THAT(builder.GetRegisterableDomains(),
              UnorderedElementsAre(kGoogleDomain));

  builder.AddRegisterableDomain(std::string(kLongETLDDomain));
  EXPECT_THAT(builder.GetRegisterableDomains(),
              UnorderedElementsAre(kGoogleDomain, kLongETLDDomain));
}

TEST(BrowsingDataFilterBuilderImplTest, ExcludeUnpartitionedCookies) {
  BrowsingDataFilterBuilderImpl builder(
      BrowsingDataFilterBuilderImpl::Mode::kDelete);

  builder.SetPartitionedStateAllowedOnly(true);

  CookieDeletionInfo delete_info =
      network::DeletionFilterToInfo(builder.BuildCookieDeletionFilter());

  // Unpartitioned cookie should NOT match.
  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      GURL("https://www.cookie.com/"),
      "__Host-A=B; Secure; SameSite=None; Path=/;", base::Time::Now(),
      absl::nullopt, absl::nullopt);
  EXPECT_TRUE(cookie);
  EXPECT_FALSE(delete_info.Matches(
      *cookie, net::CookieAccessParams{
                   net::CookieAccessSemantics::NONLEGACY, false,
                   net::CookieSamePartyStatus::kNoSamePartyEnforcement}));

  // Partitioned cookie should match.
  cookie = net::CanonicalCookie::Create(
      GURL("https://www.cookie.com/"),
      "__Host-A=B; Secure; SameSite=None; Path=/; Partitioned;",
      base::Time::Now(), absl::nullopt,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com")));
  EXPECT_TRUE(cookie);
  EXPECT_TRUE(delete_info.Matches(
      *cookie, net::CookieAccessParams{
                   net::CookieAccessSemantics::NONLEGACY, false,
                   net::CookieSamePartyStatus::kNoSamePartyEnforcement}));

  // Nonced partitioned cookie should match.
  cookie = net::CanonicalCookie::Create(
      GURL("https://www.cookie.com/"),
      "__Host-A=B; Secure; SameSite=None; Path=/;", base::Time::Now(),
      absl::nullopt,
      net::CookiePartitionKey::FromURLForTesting(
          GURL("https://toplevelsite.com"), base::UnguessableToken::Create()));
  EXPECT_TRUE(cookie);
  EXPECT_TRUE(delete_info.Matches(
      *cookie, net::CookieAccessParams{
                   net::CookieAccessSemantics::NONLEGACY, false,
                   net::CookieSamePartyStatus::kNoSamePartyEnforcement}));
}

}  // namespace content
