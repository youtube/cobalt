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
