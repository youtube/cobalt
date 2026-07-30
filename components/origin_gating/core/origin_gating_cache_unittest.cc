// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_cache.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace origin_gating {
namespace {

constexpr std::string_view kExample = "https://example.com";
constexpr std::string_view kExampleSub = "https://sub.example.com";
constexpr std::string_view kAnother = "https://another.com";

class OriginGatingCacheTest : public ::testing::TestWithParam<bool> {
 public:
  OriginGatingCacheTest() = default;
  ~OriginGatingCacheTest() override = default;

  bool is_site_scoped() const { return GetParam(); }
};

TEST_P(OriginGatingCacheTest, InitialState) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginGatingCache origin_gating_cache(is_site_scoped());
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      example, url::Origin::Create(GURL(kAnother))));
  EXPECT_FALSE(origin_gating_cache.IsNavigationConfirmedByUser(example));
}

TEST_P(OriginGatingCacheTest, AllowNavigationToSingleOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/false);

  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(origin_gating_cache.IsNavigationAllowed(
                another_origin, url::Origin::Create(GURL(kExampleSub))),
            is_site_scoped());
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("http://example.com"))));
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST_P(OriginGatingCacheTest, AllowNavigationTo_Opaque) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin opaque;
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(opaque,
                                        /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(example, opaque));
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(example, url::Origin()));
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(opaque, example));
}

TEST_P(OriginGatingCacheTest, AllowNavigationToMultipleOrigins) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin foo = url::Origin::Create(GURL("https://foo.com"));
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo({example, foo});

  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(
      origin_gating_cache.IsNavigationAllowed(another_origin, example_sub),
      is_site_scoped());
  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(another_origin, foo));
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("https://other.com"))));
}

TEST_P(OriginGatingCacheTest, IsNavigationAllowed_SameOrigin) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  OriginGatingCache origin_gating_cache(is_site_scoped());

  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(example, example));
}

TEST_P(OriginGatingCacheTest, IsNavigationAllowed_SameSite) {
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(url::Origin::Create(GURL(kExampleSub)),
                                        /*is_user_confirmed=*/false);

  EXPECT_EQ(origin_gating_cache.IsNavigationAllowed(
                url::Origin::Create(GURL(kAnother)),
                url::Origin::Create(GURL(kExample))),
            is_site_scoped());
}

TEST_P(OriginGatingCacheTest, IsNavigationAllowed_OpaqueInitiator) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/false);

  url::Origin opaque;
  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(opaque, example));
  EXPECT_EQ(origin_gating_cache.IsNavigationAllowed(opaque, example_sub),
            is_site_scoped());
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      opaque, url::Origin::Create(GURL(kAnother))));
}

TEST_P(OriginGatingCacheTest, ConfirmOrigin_Query) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));

  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_gating_cache.IsNavigationConfirmedByUser(example));
  EXPECT_EQ(origin_gating_cache.IsNavigationConfirmedByUser(example_sub),
            is_site_scoped());
  EXPECT_FALSE(origin_gating_cache.IsNavigationConfirmedByUser(
      url::Origin::Create(GURL(kAnother))));
}

TEST_P(OriginGatingCacheTest, ConfirmOrigin_AllowsNavigation) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin another_origin = url::Origin::Create(GURL(kAnother));

  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_gating_cache.IsNavigationAllowed(another_origin, example));
  EXPECT_EQ(
      origin_gating_cache.IsNavigationAllowed(another_origin, example_sub),
      is_site_scoped());
  EXPECT_FALSE(origin_gating_cache.IsNavigationAllowed(
      another_origin, url::Origin::Create(GURL("http://other.com"))));
}

TEST_P(OriginGatingCacheTest, ConfirmOrigin_Opaque) {
  const url::Origin opaque;

  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(opaque,
                                        /*is_user_confirmed=*/true);

  EXPECT_TRUE(origin_gating_cache.IsNavigationConfirmedByUser(opaque));
  EXPECT_FALSE(origin_gating_cache.IsNavigationConfirmedByUser(url::Origin()));
}

TEST_P(OriginGatingCacheTest,
       ConfirmOrigin_AllowsNavigation_RemembersConfirmation) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));

  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/false);
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/true);
  origin_gating_cache.AllowNavigationTo(example,
                                        /*is_user_confirmed=*/false);

  EXPECT_TRUE(origin_gating_cache.IsNavigationConfirmedByUser(example));
  EXPECT_EQ(origin_gating_cache.IsNavigationConfirmedByUser(example_sub),
            is_site_scoped());
}

TEST_P(OriginGatingCacheTest, GetSizeMetrics) {
  const url::Origin example = url::Origin::Create(GURL(kExample));
  const url::Origin example_sub = url::Origin::Create(GURL(kExampleSub));
  const url::Origin another = url::Origin::Create(GURL(kAnother));
  OriginGatingCache origin_gating_cache(is_site_scoped());
  origin_gating_cache.AllowNavigationTo(example, /*is_user_confirmed=*/true);
  origin_gating_cache.AllowNavigationTo(example_sub,
                                        /*is_user_confirmed=*/true);
  origin_gating_cache.AllowNavigationTo(another, /*is_user_confirmed=*/false);

  OriginGatingCache::SizeMetrics metrics = origin_gating_cache.GetSizeMetrics();

  EXPECT_EQ(metrics.allow_list_size, is_site_scoped() ? 2u : 3u);
  EXPECT_EQ(metrics.confirmed_list_size, is_site_scoped() ? 1u : 2u);
}

INSTANTIATE_TEST_SUITE_P(, OriginGatingCacheTest, ::testing::Bool());

}  // namespace
}  // namespace origin_gating
