// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/consent_kit/consent_kit_url_builder.h"

#include <string>

#include "base/base64.h"
#include "base/uuid.h"
#include "components/contextual_search/consent_kit/proto/privacy_primitive_config.pb.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {

TEST(ConsentKitUrlBuilderTest, BuildMinimalUrl) {
  ConsentKitUrlBuilder builder;
  builder.SetSessionIndex(1);
  builder.SetLocale("fr");

  GURL url = builder.Build();

  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.scheme(), "https");
  EXPECT_EQ(url.host(), "consent.google.com");
  EXPECT_EQ(url.path(), "/signedin/embedded/datasets");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &value));
  EXPECT_EQ(value, "fr");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "authuser", &value));
  EXPECT_EQ(value, "1");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "origin", &value));
  EXPECT_EQ(value, "chrome-untrusted://drive-picker-host");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));
  EXPECT_FALSE(value.empty());

  std::string decoded_ppc;
  ASSERT_TRUE(base::Base64Decode(value, &decoded_ppc));
  identity_consent::PrivacyPrimitiveConfig config;
  ASSERT_TRUE(config.ParseFromString(decoded_ppc));
  EXPECT_EQ(config.presentation_params().locale(), "fr");

  EXPECT_TRUE(config.has_session_info());
  EXPECT_TRUE(base::Uuid::ParseLowercase(config.session_info().session_id())
                  .is_valid());
}

TEST(ConsentKitUrlBuilderTest, BuildFullUrl) {
  ConsentKitUrlBuilder builder;
  builder.SetSessionIndex(2);
  builder.SetLocale("ja");
  builder.SetFlowId(987);
  builder.SetProductId(654);
  builder.SetEntrypointId("picker_entrypoint");
  builder.SetHostOrigin("chrome-untrusted://another-host");

  GURL url = builder.Build();

  ASSERT_TRUE(url.is_valid());
  EXPECT_EQ(url.scheme(), "https");
  EXPECT_EQ(url.host(), "consent.google.com");
  EXPECT_EQ(url.path(), "/signedin/embedded/datasets");

  std::string value;
  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "hl", &value));
  EXPECT_EQ(value, "ja");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "authuser", &value));
  EXPECT_EQ(value, "2");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "origin", &value));
  EXPECT_EQ(value, "chrome-untrusted://another-host");

  EXPECT_TRUE(net::GetValueForKeyInQuery(url, "ppc", &value));
  EXPECT_FALSE(value.empty());

  std::string decoded_ppc;
  ASSERT_TRUE(base::Base64Decode(value, &decoded_ppc));
  identity_consent::PrivacyPrimitiveConfig config;
  ASSERT_TRUE(config.ParseFromString(decoded_ppc));

  EXPECT_EQ(config.presentation_params().locale(), "ja");
  EXPECT_EQ(config.flow_params().flow_id(), 987);
  EXPECT_EQ(config.product_entry_point().product_id(), 654);
  EXPECT_EQ(config.product_entry_point().entrypoint_id(), "picker_entrypoint");

  EXPECT_TRUE(config.has_session_info());
  EXPECT_TRUE(base::Uuid::ParseLowercase(config.session_info().session_id())
                  .is_valid());
}

}  // namespace drive
