// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/badging/badge_manager_delegate.h"
#include "chrome/browser/badging/test_badge_manager_delegate.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

using badging::BadgeManager;
using badging::BadgeManagerDelegate;

namespace {

typedef std::pair<GURL, absl::optional<int>> SetBadgeAction;

constexpr uint64_t kBadgeContents = 1;
const web_app::AppId kAppId = "1";

class TestBadgeManager : public BadgeManager {
 public:
  TestBadgeManager(Profile* profile, web_app::WebAppSyncBridge* sync_bridge)
      : BadgeManager(profile, sync_bridge) {}

  ~TestBadgeManager() override = default;
  TestBadgeManager(const TestBadgeManager&) = delete;
  TestBadgeManager& operator=(const TestBadgeManager&) = delete;
};

}  // namespace

namespace badging {

class BadgeManagerUnittest : public ::testing::Test {
 public:
  BadgeManagerUnittest() = default;

  BadgeManagerUnittest(const BadgeManagerUnittest&) = delete;
  BadgeManagerUnittest& operator=(const BadgeManagerUnittest&) = delete;

  ~BadgeManagerUnittest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    builder.SetIsMainProfile(true);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
    profile_ = builder.Build();

    provider_ = web_app::FakeWebAppProvider::Get(profile());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());

    badge_manager_ = std::make_unique<TestBadgeManager>(
        profile(), &provider().sync_bridge_unsafe());

    // Delegate lifetime is managed by BadgeManager
    auto owned_delegate = std::make_unique<TestBadgeManagerDelegate>(
        profile_.get(), &badge_manager());
    delegate_ = owned_delegate.get();
    badge_manager().SetDelegate(std::move(owned_delegate));
  }

  void TearDown() override { profile_.reset(); }

  TestBadgeManagerDelegate* delegate() { return delegate_; }

  BadgeManager& badge_manager() const { return *badge_manager_; }

  Profile* profile() const { return profile_.get(); }

  web_app::WebAppProvider& provider() { return *provider_; }

 private:
  raw_ptr<TestBadgeManagerDelegate> delegate_;
  raw_ptr<web_app::FakeWebAppProvider> provider_;

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<BadgeManager> badge_manager_;
};

TEST_F(BadgeManagerUnittest, SetFlagBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager().SetBadgeForTesting(kAppId, absl::nullopt, &test_recorder);

  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kSetFlagBadge);

  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kAppId, delegate()->set_badges().front().first);
  EXPECT_EQ(absl::nullopt, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForApp) {
  ukm::TestUkmRecorder test_recorder;
  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), &test_recorder);
  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(entries[0],
                                  ukm::builders::Badging::kUpdateAppBadgeName,
                                  kSetNumericBadge);
  EXPECT_EQ(1UL, delegate()->set_badges().size());
  EXPECT_EQ(kAppId, delegate()->set_badges().front().first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges().front().second);
}

TEST_F(BadgeManagerUnittest, SetBadgeForMultipleApps) {
  const web_app::AppId kOtherAppId = "2";
  constexpr uint64_t kOtherContents = 2;

  std::vector<web_app::AppId> updated_apps;
  web_app::WebAppTestRegistryObserverAdapter observer(
      &provider().registrar_unsafe());
  observer.SetWebAppLastBadgingTimeChangedDelegate(base::BindLambdaForTesting(
      [&updated_apps](const web_app::AppId& app_id, const base::Time& time) {
        updated_apps.push_back(app_id);
      }));

  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(kOtherAppId,
                                     absl::make_optional(kOtherContents),
                                     ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kOtherAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kOtherContents, delegate()->set_badges()[1].second);

  EXPECT_EQ(2UL, updated_apps.size());
  EXPECT_EQ(kAppId, updated_apps[0]);
  EXPECT_EQ(kOtherAppId, updated_apps[1]);
}

TEST_F(BadgeManagerUnittest, SetBadgeForAppAfterClear) {
  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());

  EXPECT_EQ(2UL, delegate()->set_badges().size());

  EXPECT_EQ(kAppId, delegate()->set_badges()[0].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[0].second);

  EXPECT_EQ(kAppId, delegate()->set_badges()[1].first);
  EXPECT_EQ(kBadgeContents, delegate()->set_badges()[1].second);
}

TEST_F(BadgeManagerUnittest, ClearBadgeForBadgedApp) {
  ukm::TestUkmRecorder test_recorder;

  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, &test_recorder);
  auto entries =
      test_recorder.GetEntriesByName(ukm::builders::Badging::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  test_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Badging::kUpdateAppBadgeName, kClearBadge);
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());
  EXPECT_EQ(kAppId, delegate()->cleared_badges().front());
}

#if !BUILDFLAG(IS_CHROMEOS_LACROS)
TEST_F(BadgeManagerUnittest, BadgingMultipleProfiles) {
  std::unique_ptr<Profile> other_profile = std::make_unique<TestingProfile>();
  web_app::FakeWebAppProvider* new_provider =
      web_app::FakeWebAppProvider::Get(other_profile.get());
  web_app::test::AwaitStartWebAppProviderAndSubsystems(other_profile.get());

  auto other_badge_manager = std::make_unique<TestBadgeManager>(
      other_profile.get(), &new_provider->sync_bridge_unsafe());

  auto owned_other_delegate = std::make_unique<TestBadgeManagerDelegate>(
      other_profile.get(), other_badge_manager.get());
  auto* other_delegate = owned_other_delegate.get();
  other_badge_manager->SetDelegate(std::move(owned_other_delegate));

  std::vector<web_app::AppId> updated_apps;
  std::vector<web_app::AppId> other_updated_apps;
  web_app::WebAppTestRegistryObserverAdapter other_observer(
      &new_provider->registrar_unsafe());
  other_observer.SetWebAppLastBadgingTimeChangedDelegate(
      base::BindLambdaForTesting(
          [&other_updated_apps](const web_app::AppId& app_id,
                                const base::Time& time) {
            other_updated_apps.push_back(app_id);
          }));
  web_app::WebAppTestRegistryObserverAdapter observer(
      &provider().registrar_unsafe());
  observer.SetWebAppLastBadgingTimeChangedDelegate(base::BindLambdaForTesting(
      [&updated_apps](const web_app::AppId& app_id, const base::Time& time) {
        updated_apps.push_back(app_id);
      }));

  other_badge_manager->SetBadgeForTesting(kAppId, absl::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  other_badge_manager->SetBadgeForTesting(kAppId, absl::nullopt,
                                          ukm::TestUkmRecorder::Get());
  other_badge_manager->ClearBadgeForTesting(kAppId,
                                            ukm::TestUkmRecorder::Get());

  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());

  EXPECT_EQ(3UL, other_delegate->set_badges().size());
  EXPECT_EQ(0UL, delegate()->set_badges().size());

  EXPECT_EQ(1UL, other_delegate->cleared_badges().size());
  EXPECT_EQ(1UL, delegate()->cleared_badges().size());

  EXPECT_EQ(kAppId, other_delegate->set_badges().back().first);
  EXPECT_EQ(absl::nullopt, other_delegate->set_badges().back().second);

  EXPECT_EQ(1UL, updated_apps.size());
  EXPECT_EQ(kAppId, updated_apps[0]);

  EXPECT_FALSE(other_updated_apps.empty());
  EXPECT_EQ(kAppId, other_updated_apps[0]);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

// Tests methods which call into the badge manager delegate do not crash when
// the delegate is unset.
TEST_F(BadgeManagerUnittest, BadgingWithNoDelegateDoesNotCrash) {
  badge_manager().SetDelegate(nullptr);

  badge_manager().SetBadgeForTesting(kAppId, absl::nullopt,
                                     ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
}

// Tests methods which use the web app sync_bridge do not crash when web
// apps aren't supported (and thus sync_bridge is null).
TEST_F(BadgeManagerUnittest, BadgingWithNoSyncBridgeDoesNotCrash) {
  badge_manager().SetSyncBridgeForTesting(nullptr);

  badge_manager().SetBadgeForTesting(kAppId, absl::nullopt,
                                     ukm::TestUkmRecorder::Get());
  badge_manager().SetBadgeForTesting(
      kAppId, absl::make_optional(kBadgeContents), ukm::TestUkmRecorder::Get());
  badge_manager().ClearBadgeForTesting(kAppId, ukm::TestUkmRecorder::Get());
}

}  // namespace badging
