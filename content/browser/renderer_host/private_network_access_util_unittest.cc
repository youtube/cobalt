// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <ostream>
#include <tuple>
#include <vector>

#include "content/browser/renderer_host/private_network_access_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "content/common/features.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using AddressSpace = network::mojom::IPAddressSpace;
using RequestContext = PrivateNetworkRequestContext;
using Policy = network::mojom::PrivateNetworkRequestPolicy;

using ::testing::ElementsAreArray;

// Self-descriptive constants for `is_web_secure_context`.
constexpr bool kNonSecure = false;
constexpr bool kSecure = true;

// Input arguments to `DerivePrivateNetworkRequestPolicy()`.
struct DerivePolicyInput {
  bool is_web_secure_context;
  AddressSpace address_space;
  RequestContext request_context;

  // Helper for comparison operators.
  std::tuple<bool, AddressSpace, RequestContext> ToTuple() const {
    return {is_web_secure_context, address_space, request_context};
  }

  bool operator==(const DerivePolicyInput& other) const {
    return ToTuple() == other.ToTuple();
  }

  // Allows using inputs as keys of a map.
  bool operator<(const DerivePolicyInput& other) const {
    return ToTuple() < other.ToTuple();
  }
};

base::StringPiece RequestContextToStringPiece(RequestContext request_context) {
  switch (request_context) {
    case RequestContext::kSubresource:
      return "subresource";
    case RequestContext::kIframe:
      return "iframe";
    case RequestContext::kWorker:
      return "worker";
  }
}

// For ease of debugging.
std::ostream& operator<<(std::ostream& out, const DerivePolicyInput& input) {
  return out << "{ " << input.address_space << ", "
             << (input.is_web_secure_context ? "secure" : "non-secure") << ", "
             << RequestContextToStringPiece(input.request_context) << " }";
}

Policy DerivePolicy(DerivePolicyInput input) {
  return DerivePrivateNetworkRequestPolicy(
      input.address_space, input.is_web_secure_context, input.request_context);
}

// Maps inputs to their default output (all feature flags left untouched).
std::map<DerivePolicyInput, Policy> DefaultPolicyMap() {
  return {
      //
      // `RequestContext::kSubresource`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kSubresource},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kLocal, RequestContext::kSubresource},
          Policy::kBlock,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kSubresource},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kSubresource},
          Policy::kPreflightWarn,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kSubresource},
          Policy::kPreflightWarn,
      },
      {
          {kSecure, AddressSpace::kLocal, RequestContext::kSubresource},
          Policy::kPreflightWarn,
      },
      //
      // `RequestContext::kWorker`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kNonSecure, AddressSpace::kLocal, RequestContext::kWorker},
          Policy::kWarn,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kWorker},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kWorker},
          Policy::kPreflightWarn,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kWorker},
          Policy::kPreflightWarn,
      },
      {
          {kSecure, AddressSpace::kLocal, RequestContext::kWorker},
          Policy::kPreflightWarn,
      },
      //
      // `RequestContext::kIframe`
      //
      {
          {kNonSecure, AddressSpace::kUnknown, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPublic, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kPrivate, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kNonSecure, AddressSpace::kLocal, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kUnknown, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPublic, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kPrivate, RequestContext::kIframe},
          Policy::kAllow,
      },
      {
          {kSecure, AddressSpace::kLocal, RequestContext::kIframe},
          Policy::kAllow,
      },
  };
}

std::map<DerivePolicyInput, Policy> AllAllowMap() {
  std::map<DerivePolicyInput, Policy> result = DefaultPolicyMap();
  for (auto& entry : result) {
    entry.second = Policy::kAllow;
  }
  return result;
}

// Runs `DerivePolicy()` on each key and compares the result to the map value.
void TestPolicyMap(const std::map<DerivePolicyInput, Policy>& expected) {
  for (const auto& [input, policy] : expected) {
    EXPECT_EQ(DerivePolicy(input), policy) << input;
  }
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicy) {
  TestPolicyMap(DefaultPolicyMap());
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyBlockFromInsecurePrivate) {
  base::test::ScopedFeatureList feature_list(
      features::kBlockInsecurePrivateNetworkRequestsFromPrivate);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  // Only need to override non-worker case because workers are by default
  // warnings only.
  expected[{kNonSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyBlockFromInsecureUnknown) {
  base::test::ScopedFeatureList feature_list(
      features::kBlockInsecurePrivateNetworkRequestsFromUnknown);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kNonSecure, AddressSpace::kUnknown, RequestContext::kSubresource}] =
      Policy::kBlock;
  // Workers are currently in warning-only mode.
  expected[{kNonSecure, AddressSpace::kUnknown, RequestContext::kWorker}] =
      Policy::kWarn;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyNoPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {}, {features::kPrivateNetworkAccessSendPreflights});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kSubresource}] =
      Policy::kAllow;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kAllow;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kSubresource}] =
      Policy::kAllow;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kWorker}] =
      Policy::kAllow;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kWorker}] =
      Policy::kAllow;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kWorker}] =
      Policy::kAllow;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyRespectPreflightResults) {
  base::test::ScopedFeatureList feature_list(
      features::kPrivateNetworkAccessRespectPreflightResults);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyWorkers) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kPrivateNetworkAccessForWorkers},
      {features::kPrivateNetworkAccessForWorkersWarningOnly});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kWorker}] =
      Policy::kBlock;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kWorker}] =
      Policy::kBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyWorkersWithPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kPrivateNetworkAccessForWorkers,
          features::kPrivateNetworkAccessRespectPreflightResults,
      },
      {features::kPrivateNetworkAccessForWorkersWarningOnly});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();

  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kWorker}] =
      Policy::kBlock;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kWorker}] =
      Policy::kBlock;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kWorker}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kWorker}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kWorker}] =
      Policy::kPreflightBlock;

  // Subresources are also affected by preflight enforcement.
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyDisableWebSecurity) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableWebSecurity);

  TestPolicyMap(AllAllowMap());
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyIframesWarningOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kPrivateNetworkAccessForIframes,
          features::kPrivateNetworkAccessForIframesWarningOnly,
      },
      {});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kPreflightWarn;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest,
     DerivePolicyIframesWarningOnlyWithPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kPrivateNetworkAccessForIframes,
          features::kPrivateNetworkAccessForIframesWarningOnly,
          features::kPrivateNetworkAccessRespectPreflightResults,
      },
      {});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();

  // Despite preflight enforcement being enabled for subresources, iframes are
  // still in warning-only mode.

  expected[{kSecure, AddressSpace::kPublic, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;

  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kPreflightWarn;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyIframes) {
  base::test::ScopedFeatureList feature_list(
      features::kPrivateNetworkAccessForIframes);

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();
  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kBlock;
  expected[{kNonSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kBlock;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kPreflightWarn;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kPreflightWarn;

  TestPolicyMap(expected);
}

TEST(PrivateNetworkAccessUtilTest, DerivePolicyIframesWithPreflights) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {
          features::kPrivateNetworkAccessForIframes,
          features::kPrivateNetworkAccessRespectPreflightResults,
      },
      {});

  std::map<DerivePolicyInput, Policy> expected = DefaultPolicyMap();

  expected[{kNonSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kBlock;
  expected[{kNonSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kWarn;
  expected[{kNonSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kBlock;
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kIframe}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kIframe}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kIframe}] =
      Policy::kPreflightBlock;

  // Subresources are also affected by preflight enforcement.
  expected[{kSecure, AddressSpace::kPublic, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kPrivate, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;
  expected[{kSecure, AddressSpace::kLocal, RequestContext::kSubresource}] =
      Policy::kPreflightBlock;

  TestPolicyMap(expected);
}

}  // namespace
}  // namespace content
