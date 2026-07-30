// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_config_service_defaults.h"

#include <memory>
#include <string_view>

#include "net/base/ech_mode.h"
#include "net/ssl/ech_mode_getter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

class MockEchModeGetter : public EchModeGetter {
 public:
  MOCK_METHOD(EchMode,
              GetEchMode,
              (std::string_view hostname),
              (const, override));
};

}  // namespace

TEST(SSLConfigServiceDefaultsTest, GetEchModePassesHostname) {
  // Test fallback logic without delegate
  SSLConfigServiceDefaults service_without_delegate;
  EXPECT_EQ(EchMode::kOpportunistic,
            service_without_delegate.GetEchMode("example.com"));

  // Test forwarding to delegate
  auto mock_getter = std::make_unique<MockEchModeGetter>();
  EXPECT_CALL(*mock_getter, GetEchMode(std::string_view("example.com")))
      .WillOnce(testing::Return(EchMode::kStrict));

  SSLConfigServiceDefaults service_with_delegate(std::move(mock_getter));
  EXPECT_EQ(EchMode::kStrict, service_with_delegate.GetEchMode("example.com"));
}

}  // namespace net
