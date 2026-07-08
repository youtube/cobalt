// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/browser/client_hint_headers/cobalt_header_value_provider.h"

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_STARBOARD)
#include "starboard/common/system_property.h"
#endif

namespace cobalt {
namespace browser {

class CobaltHeaderValueProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Clear any leftover test state before each test run to ensure isolation.
    CobaltHeaderValueProvider::GetInstance()->ClearHeaderValuesForTesting();
  }

  void TearDown() override {
    // Clean up test state after each test case.
    CobaltHeaderValueProvider::GetInstance()->ClearHeaderValuesForTesting();
  }
};

// Verifies that GetInstance() returns a non-null, valid process-wide singleton.
TEST_F(CobaltHeaderValueProviderTest, GetInstanceReturnsSameInstance) {
  // Step 1: Obtain two instance pointers from GetInstance().
  CobaltHeaderValueProvider* instance1 =
      CobaltHeaderValueProvider::GetInstance();
  CobaltHeaderValueProvider* instance2 =
      CobaltHeaderValueProvider::GetInstance();

  // Step 2: Assert both pointers are non-null and point to identical memory
  // address.
  ASSERT_NE(nullptr, instance1);
  ASSERT_EQ(instance1, instance2);
}

// Verifies setting and retrieving custom client hint header key-value pairs.
TEST_F(CobaltHeaderValueProviderTest, SetAndGetHeaderValues) {
  // Step 1: Obtain singleton instance.
  CobaltHeaderValueProvider* provider =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, provider);

  // Step 2: Insert two distinct header key-value pairs.
  provider->SetHeaderValue("Cobalt-Client-Hint-Header-A", "Test-Value-A");
  provider->SetHeaderValue("Cobalt-Client-Hint-Header-B", "Test-Value-B");

  // Step 3: Retrieve current header map and verify both key-value pairs exist
  // and match.
  const auto headers = provider->GetHeaderValues();

  auto it_a = headers.find("Cobalt-Client-Hint-Header-A");
  ASSERT_NE(headers.end(), it_a);
  EXPECT_EQ("Test-Value-A", it_a->second);

  auto it_b = headers.find("Cobalt-Client-Hint-Header-B");
  ASSERT_NE(headers.end(), it_b);
  EXPECT_EQ("Test-Value-B", it_b->second);
}

// Verifies that updating an existing header key overwrites the previous value
// cleanly.
TEST_F(CobaltHeaderValueProviderTest, UpdateHeaderValue) {
  // Step 1: Obtain singleton instance.
  CobaltHeaderValueProvider* provider =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, provider);

  // Step 2: Set initial header value and safely verify via find() without
  // std::map::at().
  provider->SetHeaderValue("Cobalt-Client-Hint-Header-Update", "Value1");
  const auto headers1 = provider->GetHeaderValues();
  auto it1 = headers1.find("Cobalt-Client-Hint-Header-Update");
  ASSERT_NE(headers1.end(), it1);
  EXPECT_EQ("Value1", it1->second);

  // Step 3: Overwrite header value and verify updated value.
  provider->SetHeaderValue("Cobalt-Client-Hint-Header-Update", "Value2");
  const auto headers2 = provider->GetHeaderValues();
  auto it2 = headers2.find("Cobalt-Client-Hint-Header-Update");
  ASSERT_NE(headers2.end(), it2);
  EXPECT_EQ("Value2", it2->second);
}

#if BUILDFLAG(IS_STARBOARD)
// Verifies that kCobaltCertScopeHeaderName is properly populated from the
// platform system property kSbSystemPropertyCertificationScope on Starboard
// (b/530151533).
TEST_F(CobaltHeaderValueProviderTest, CertScopeHeaderMatchesPlatformProperty) {
  // Step 1: Obtain singleton instance and re-initialize from system properties.
  CobaltHeaderValueProvider* provider =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, provider);

  // Step 2: Query system property directly on the test platform.
  std::string cert_scope =
      starboard::GetSystemPropertyString(kSbSystemPropertyCertificationScope);

  // Step 3: Retrieve header map and assert:
  // - If cert_scope is empty, header must NOT be present.
  // - If cert_scope is non-empty, header must be present and match.
  const auto headers = provider->GetHeaderValues();
  auto it = headers.find(kCobaltCertScopeHeaderName);

  if (cert_scope.empty()) {
    EXPECT_EQ(headers.end(), it);
  } else {
    ASSERT_NE(headers.end(), it);
    EXPECT_EQ(cert_scope, it->second);
  }
}
#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace browser
}  // namespace cobalt
