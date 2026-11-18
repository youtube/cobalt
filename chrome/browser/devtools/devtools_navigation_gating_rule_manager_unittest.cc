// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_navigation_gating_rule_manager.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

TEST(DevToolsNavigationGatingRuleManagerTest, EmptyRulesAllowsAll) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("{}");
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("http://a.com")));
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://b.com/foo")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, AllowlistRestrictsNavigations) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": ["https://a.com", "http://b.com"]})");

  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://a.com")));
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://a.com/foo")));
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("http://b.com:80/bar")));

  EXPECT_FALSE(
      manager.IsNavigationAllowed(GURL("http://a.com")));  // Scheme mismatch
  EXPECT_FALSE(
      manager.IsNavigationAllowed(GURL("https://c.com")));  // Host mismatch
}

TEST(DevToolsNavigationGatingRuleManagerTest, EmptyAllowlistBlocksAll) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": []})");

  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("https://a.com")));
  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("https://b.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, AllowlistWildcardDomain) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"allowlist": ["https://[*.]example.com"]})");

  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://example.com")));
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://sub.example.com")));
  EXPECT_TRUE(
      manager.IsNavigationAllowed(GURL("https://deep.sub.example.com/foo")));

  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("http://example.com")));
  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("https://notexample.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest, BlocklistRestrictsNavigations) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(
          R"({"blocklist": ["https://blockedsite.com"]})");

  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://allowedsite.com")));
  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("https://blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     SpecificityBlocklistOverridesAllowlist) {
  // blocklist pattern is more specific
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://[*.]blockedsite.com", "https://allowedsite.com"],
        "blocklist": ["https://sub.blockedsite.com"]
      })");

  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://allowedsite.com")));
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://blockedsite.com")));
  EXPECT_TRUE(
      manager.IsNavigationAllowed(GURL("https://another.blockedsite.com")));
  EXPECT_FALSE(
      manager.IsNavigationAllowed(GURL("https://sub.blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     SpecificityAllowlistOverridesBlocklist) {
  // allowlist pattern is more specific
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting(R"({
        "allowlist": ["https://allow.blockedsite.com"],
        "blocklist": ["https://[*.]blockedsite.com"]
      })");

  EXPECT_FALSE(manager.IsNavigationAllowed(GURL("https://blockedsite.com")));
  EXPECT_FALSE(
      manager.IsNavigationAllowed(GURL("https://sub.blockedsite.com")));
  EXPECT_TRUE(
      manager.IsNavigationAllowed(GURL("https://allow.blockedsite.com")));
}

TEST(DevToolsNavigationGatingRuleManagerTest,
     MalformedJsonRulesFallbackToEmpty) {
  DevToolsNavigationGatingRuleManager manager =
      DevToolsNavigationGatingRuleManager::CreateForTesting("invalid-json");
  EXPECT_TRUE(manager.IsNavigationAllowed(GURL("https://any.com")));
}

}  // namespace
