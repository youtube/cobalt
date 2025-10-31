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

#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

TEST(DeepLinkManagerTest, GetInstanceReturnsNonNull) {
  DeepLinkManager* instance = DeepLinkManager::GetInstance();
  ASSERT_NE(nullptr, instance);
}

TEST(DeepLinkManagerTest, SetAndGetDeepLink) {
  auto* manager = DeepLinkManager::GetInstance();
  const std::string test_url = "https://example.com/test";
  manager->set_deep_link(test_url);
  EXPECT_EQ(test_url, manager->get_deep_link());
}

TEST(DeepLinkManagerTest, GetAndClearDeepLink) {
  auto* manager = DeepLinkManager::GetInstance();
  const std::string test_url = "https://example.com/clear";
  manager->set_deep_link(test_url);
  EXPECT_EQ(test_url, manager->GetAndClearDeepLink());
  EXPECT_EQ("", manager->get_deep_link());
}

TEST(DeepLinkManagerTest, GetAndClearDeepLink_InitiallyEmpty) {
  auto* manager = DeepLinkManager::GetInstance();
  EXPECT_EQ("", manager->GetAndClearDeepLink());
}

TEST(DeepLinkManagerTest, GetAndClearDeepLink_MultipleCalls) {
  auto* manager = DeepLinkManager::GetInstance();
  const std::string test_url = "cobalt://deeplink";
  manager->set_deep_link(test_url);
  EXPECT_EQ(test_url, manager->GetAndClearDeepLink());
  EXPECT_EQ("", manager->GetAndClearDeepLink());
}

}  // namespace browser
}  // namespace cobalt
