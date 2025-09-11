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

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

TEST(CobaltHeaderValueProviderTest, GetInstanceReturnsSameInstance) {
  CobaltHeaderValueProvider* instance1 =
      CobaltHeaderValueProvider::GetInstance();
  CobaltHeaderValueProvider* instance2 =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, instance1);
  ASSERT_EQ(instance1, instance2);
}

TEST(CobaltHeaderValueProviderTest, SetAndGetHeaderValues) {
  CobaltHeaderValueProvider* provider =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, provider);

  provider->SetHeaderValue("Cobalt-Client-Hint-Header-A", "Test-Value-A");
  provider->SetHeaderValue("Cobalt-Client-Hint-Header-B", "Test-Value-B");

  const HeaderMap& headers = provider->GetHeaderValues();

  auto it_a = headers.find("Cobalt-Client-Hint-Header-A");
  ASSERT_NE(headers.end(), it_a);
  EXPECT_EQ("Test-Value-A", it_a->second);

  auto it_b = headers.find("Cobalt-Client-Hint-Header-B");
  ASSERT_NE(headers.end(), it_b);
  EXPECT_EQ("Test-Value-B", it_b->second);
}

TEST(CobaltHeaderValueProviderTest, UpdateHeaderValue) {
  CobaltHeaderValueProvider* provider =
      CobaltHeaderValueProvider::GetInstance();
  ASSERT_NE(nullptr, provider);

  provider->SetHeaderValue("Cobalt-Client-Hint-Header", "Value1");
  ASSERT_EQ("Value1",
            provider->GetHeaderValues().at("Cobalt-Client-Hint-Header"));

  // Update the value
  provider->SetHeaderValue("Cobalt-Client-Hint-Header", "Value2");
  const HeaderMap& headers = provider->GetHeaderValues();
  auto it = headers.find("Cobalt-Client-Hint-Header");
  ASSERT_NE(headers.end(), it);
  EXPECT_EQ("Value2", it->second);
}

}  // namespace browser
}  // namespace cobalt
