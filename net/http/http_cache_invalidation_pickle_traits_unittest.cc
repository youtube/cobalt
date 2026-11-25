// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache_invalidation_pickle_traits.h"

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "net/base/pickle.h"
#include "net/base/pickle_base_types.h"
#include "net/base/pickle_url_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {
namespace {

TEST(HttpCacheInvalidationPickleTraitsTest, RoundTripAndPickleSize) {
  HttpCache::InvalidationFilter original;
  original.begin_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  original.end_time =
      base::Time::FromSecondsSinceUnixEpoch(1000);  // Point in time
  original.filter_type = UrlFilterType::kTrueIfMatches;
  original.origins = {url::Origin::Create(GURL("https://example.com"))};
  original.domains = {"test.org", "chromium.org"};

  base::Pickle pickle;
  WriteToPickle(pickle, original);

  EXPECT_LE(pickle.payload_size(),
            PickleTraits<HttpCache::InvalidationFilter>::PickleSize(original));

  base::PickleIterator iter(pickle);
  std::optional<HttpCache::InvalidationFilter> deserialized =
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter);

  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(original, *deserialized);
}

TEST(HttpCacheInvalidationPickleTraitsTest, EmptyCollectionsRoundTrip) {
  HttpCache::InvalidationFilter original;
  original.begin_time = base::Time::FromSecondsSinceUnixEpoch(1000);
  original.end_time = base::Time::FromSecondsSinceUnixEpoch(2000);
  original.filter_type = UrlFilterType::kFalseIfMatches;
  original.origins = {};
  original.domains = {};

  base::Pickle pickle;
  WriteToPickle(pickle, original);

  base::PickleIterator iter(pickle);
  std::optional<HttpCache::InvalidationFilter> deserialized =
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter);

  ASSERT_TRUE(deserialized.has_value());
  EXPECT_EQ(original, *deserialized);
}

TEST(HttpCacheInvalidationPickleTraitsTest, TruncatedDataReturnsNullopt) {
  base::Pickle pickle;
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(1000));

  base::PickleIterator iter(pickle);
  EXPECT_FALSE(
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter).has_value());
}

TEST(HttpCacheInvalidationPickleTraitsTest, InvalidTimeRangeReturnsNullopt) {
  base::Pickle pickle;
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(2000));
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(1000));
  WriteToPickle(pickle, static_cast<int>(UrlFilterType::kTrueIfMatches));
  WriteToPickle(pickle, std::vector<std::string>{});
  WriteToPickle(pickle, std::vector<std::string>{});

  base::PickleIterator iter(pickle);
  EXPECT_FALSE(
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter).has_value());
}

TEST(HttpCacheInvalidationPickleTraitsTest, InvalidEnumReturnsNullopt) {
  base::Pickle pickle;
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(1000));
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(2000));
  WriteToPickle(pickle, int{999});  // Invalid UrlFilterType enum value

  base::PickleIterator iter(pickle);
  EXPECT_FALSE(
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter).has_value());
}

TEST(HttpCacheInvalidationPickleTraitsTest,
     ExceedsMaxCollectionSizeReturnsNullopt) {
  base::Pickle pickle;
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(1000));
  WriteToPickle(pickle, base::Time::FromSecondsSinceUnixEpoch(2000));
  WriteToPickle(pickle, static_cast<int>(UrlFilterType::kTrueIfMatches));

  constexpr size_t kMax =
      PickleTraits<HttpCache::InvalidationFilter>::kMaxCollectionSize;
  std::vector<std::string> huge_domains;
  huge_domains.reserve(kMax + 1);
  for (size_t i = 0; i < kMax + 1; ++i) {
    huge_domains.push_back("domain" + base::NumberToString(i) + ".com");
  }
  WriteToPickle(pickle, std::vector<url::Origin>{});  // origins
  WriteToPickle(pickle, huge_domains);                // domains

  base::PickleIterator iter(pickle);
  EXPECT_FALSE(
      ReadValueFromPickle<HttpCache::InvalidationFilter>(iter).has_value());
}

}  // namespace
}  // namespace net
