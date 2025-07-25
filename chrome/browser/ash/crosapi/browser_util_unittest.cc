// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_util.h"

#include <memory>

#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_util.h"
#include "chrome/browser/ash/crosapi/environment_provider.h"
#include "chrome/browser/ash/crosapi/idle_service_ash.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/standalone_browser/browser_support.h"
#include "chromeos/ash/components/standalone_browser/lacros_availability.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "components/account_id/account_id.h"
#include "components/policy/policy_constants.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/channel.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ash::standalone_browser::LacrosAvailability;
using crosapi::browser_util::LacrosLaunchSwitchSource;
using crosapi::browser_util::LacrosSelection;
using user_manager::User;
using version_info::Channel;

namespace crosapi {

namespace {
// This implementation of RAII for LacrosAvailability is to make it easy reset
// the state between runs.
class ScopedLacrosAvailabilityCache {
 public:
  explicit ScopedLacrosAvailabilityCache(
      LacrosAvailability lacros_launch_switch) {
    SetLacrosAvailability(lacros_launch_switch);
  }
  ScopedLacrosAvailabilityCache(const ScopedLacrosAvailabilityCache&) = delete;
  ScopedLacrosAvailabilityCache& operator=(
      const ScopedLacrosAvailabilityCache&) = delete;
  ~ScopedLacrosAvailabilityCache() {
    browser_util::ClearLacrosAvailabilityCacheForTest();
    ash::standalone_browser::BrowserSupport::Shutdown();
  }

 private:
  void SetLacrosAvailability(LacrosAvailability lacros_availability) {
    policy::PolicyMap policy;
    policy.Set(
        policy::key::kLacrosAvailability, policy::POLICY_LEVEL_MANDATORY,
        policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
        base::Value(GetLacrosAvailabilityPolicyName(lacros_availability)),
        /*external_data_fetcher=*/nullptr);
    browser_util::CacheLacrosAvailability(policy);
    if (ash::standalone_browser::BrowserSupport::
            IsInitializedForPrimaryUser()) {
      ash::standalone_browser::BrowserSupport::Shutdown();
    }
    ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(policy);
  }
};

// This implementation of RAII for LacrosSelection is to make it easy reset
// the state between runs.
class ScopedLacrosSelectionCache {
 public:
  explicit ScopedLacrosSelectionCache(
      browser_util::LacrosSelectionPolicy lacros_selection) {
    SetLacrosSelection(lacros_selection);
  }
  ScopedLacrosSelectionCache(const ScopedLacrosSelectionCache&) = delete;
  ScopedLacrosSelectionCache& operator=(const ScopedLacrosSelectionCache&) =
      delete;
  ~ScopedLacrosSelectionCache() {
    browser_util::ClearLacrosSelectionCacheForTest();
  }

 private:
  void SetLacrosSelection(
      browser_util::LacrosSelectionPolicy lacros_selection) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosSelection, policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosSelectionPolicyName(lacros_selection)),
               /*external_data_fetcher=*/nullptr);
    browser_util::CacheLacrosSelection(policy);
  }
};

}  // namespace

class BrowserUtilTest : public testing::Test {
 public:
  BrowserUtilTest() = default;
  ~BrowserUtilTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
    browser_util::RegisterLocalStatePrefs(pref_service_.registry());
    ash::standalone_browser::migrator_util::RegisterLocalStatePrefs(
        pref_service_.registry());
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
  }

  void TearDown() override {
    if (ash::standalone_browser::BrowserSupport::
            IsInitializedForPrimaryUser()) {
      ash::standalone_browser::BrowserSupport::Shutdown();
    }
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
    fake_user_manager_.Reset();
    ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(
        absl::nullopt);
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    ash::standalone_browser::BrowserSupport::InitializeForPrimaryUser(
        policy::PolicyMap());
    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, &testing_profile_);
  }

  // The order of these members is relevant for both construction and
  // destruction timing.
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  TestingProfile testing_profile_;
  TestingPrefServiceSimple pref_service_;
  ash::system::FakeStatisticsProvider statistics_provider_;
};

// TODO(hidehiko): Replace with ScopedTestingLocalState.
class ScopedLocalState {
 public:
  explicit ScopedLocalState(PrefService* local_state) {
    TestingBrowserProcess::GetGlobal()->SetLocalState(local_state);
  }

  ~ScopedLocalState() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }
};

TEST_F(BrowserUtilTest, LacrosEnabledByFlag) {
  AddRegularUser("user@test.com");

  {
    // Lacros is initially disabled.
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }

  {
    // Disabling the flag disables Lacros.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        ash::standalone_browser::features::kLacrosOnly);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }

  {
    // Enabling the flag enables Lacros.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        ash::standalone_browser::features::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, LacrosDisallowedByCommandLineFlag) {
  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisallowLacros);
  AddRegularUser("user@test.com");
  EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisableDisallowedLacrosByCommandLineFlag) {
  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisallowLacros);
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kDisableDisallowLacros);
  AddRegularUser("user@test.com");
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisabledWithoutMigration) {
  // This sets `g_browser_process->local_state()` which activates the check
  // `IsProfileMigrationCompletedForUser()` inside `IsLacrosEnabled()`.
  ScopedLocalState scoped_local_state(&pref_service_);

  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  // Lacros is enabled only after profile migration for LacrosOnly mode.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});

    EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
        user, browser_util::PolicyInitState::kAfterInit));
    // Since profile migration hasn't been marked as completed, this returns
    // false.
    EXPECT_FALSE(browser_util::IsLacrosEnabled());

    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        &pref_service_, user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kCopy);

    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, IsLacrosEnabledForMigrationBeforePolicyInit) {
  // This sets `g_browser_process->local_state()` which activates the check
  // `IsProfileMigrationCompletedForUser()` inside `IsLacrosEnabled()`.
  ScopedLocalState scoped_local_state(&pref_service_);

  // Add an user.
  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  // Lacros is not enabled yet for profile migration to happen.
  EXPECT_FALSE(browser_util::IsLacrosEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));

  // Sets command line flag to emulate the situation where the Chrome
  // restart happens.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(
      ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
      ash::standalone_browser::kLacrosAvailabilityPolicyLacrosOnly);

  EXPECT_TRUE(browser_util::IsLacrosEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserUtilTest, LacrosEnabled) {
  AddRegularUser("user@test.com");

  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, ManagedAccountLacros) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  AddRegularUser("user@managedchrome.com");

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, BlockedForChildUser) {
  AccountId account_id = AccountId::FromUserEmail("user@test.com");
  const User* user = fake_user_manager_->AddChildUser(account_id);
  fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                   /*browser_restart=*/false,
                                   /*is_child=*/true);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, AshWebBrowserEnabled) {
  AddRegularUser("user@managedchrome.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  // Lacros is not allowed.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);

    EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabledForMigration(
        user, browser_util::PolicyInitState::kAfterInit));
  }

  // Lacros is allowed but not enabled.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabledForMigration(
        user, browser_util::PolicyInitState::kAfterInit));
  }

  // Lacros is allowed and enabled by flag.
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeature(
        ash::standalone_browser::features::kLacrosOnly);
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
        user, browser_util::PolicyInitState::kAfterInit));
  }

  // Lacros is allowed and enabled by policy.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

    EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
        user, browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserUtilTest, IsAshWebBrowserEnabledForMigration) {
  // Add an user.
  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  // Ash browser is enabled if Lacros is not enabled.
  EXPECT_TRUE(browser_util::IsAshWebBrowserEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));

  // Sets command line flag to emulate the situation where the Chrome
  // restart happens.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchASCII(
      ash::standalone_browser::kLacrosAvailabilityPolicySwitch,
      ash::standalone_browser::kLacrosAvailabilityPolicyLacrosOnly);

  // Ash browser is disabled if LacrosOnly is enabled.
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
      user, browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabled) {
  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@managedchrome.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

  // Lacros is allowed and enabled and is the only browser by policy.

  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
      user, browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserUtilTest, IsAshWebBrowserDisabledByFlags) {
  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);
  EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());

  // Just enabling LacrosOnly feature is enough.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      ash::standalone_browser::features::kLacrosOnly);
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
      user, browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserUtilTest, LacrosOnlyBrowserByFlags) {
  AddRegularUser("user@test.com");
  EXPECT_FALSE(browser_util::IsLacrosEnabled());

  // Just setting LacrosOnly should work.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosDisabledForOldHardware) {
  AddRegularUser("user@test.com");
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  EXPECT_TRUE(browser_util::IsLacrosEnabled());

  ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(false);
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
  ash::standalone_browser::BrowserSupport::SetCpuSupportedForTesting(true);
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
}

TEST_F(BrowserUtilTest, LacrosOnlyBrowserAllowed) {
  AddRegularUser("user@test.com");
  EXPECT_TRUE(browser_util::IsLacrosOnlyBrowserAllowed());
}

TEST_F(BrowserUtilTest, ManagedAccountLacrosPrimary) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  AddRegularUser("user@managedchrome.com");

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
    EXPECT_FALSE(browser_util::IsLacrosOnlyBrowserAllowed());
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
  }

  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
    EXPECT_TRUE(browser_util::IsLacrosOnlyBrowserAllowed());
    EXPECT_TRUE(browser_util::IsLacrosEnabled());
  }
}

TEST_F(BrowserUtilTest, MetadataMissing) {
  EXPECT_FALSE(browser_util::DoesMetadataSupportNewAccountManager(nullptr));
}

TEST_F(BrowserUtilTest, MetadataMissingVersion) {
  std::string json_string = R"###(
   {
     "content": {
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataVersionBadFormat) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataOldVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "91.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_FALSE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, MetadataNewVersion) {
  std::string json_string = R"###(
   {
     "content": {
       "version": "9999.0.4469.5"
     },
     "metadata_version": 1
   }
  )###";
  absl::optional<base::Value> value = base::JSONReader::Read(json_string);
  EXPECT_TRUE(
      browser_util::DoesMetadataSupportNewAccountManager(&value.value()));
}

TEST_F(BrowserUtilTest, GetMissingDataVer) {
  std::string user_id_hash = "1234";
  base::Version version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(BrowserUtilTest, GetCorruptDataVer) {
  base::Value::Dict dictionary_value;
  std::string user_id_hash = "1234";
  dictionary_value.Set(user_id_hash, "corrupted");
  pref_service_.Set(browser_util::kDataVerPref,
                    base::Value(std::move(dictionary_value)));
  base::Version version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_FALSE(version.IsValid());
}

TEST_F(BrowserUtilTest, GetDataVer) {
  base::Value::Dict dictionary_value;
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  dictionary_value.Set(user_id_hash, version.GetString());
  pref_service_.Set(browser_util::kDataVerPref,
                    base::Value(std::move(dictionary_value)));

  base::Version result_version =
      browser_util::GetDataVer(&pref_service_, user_id_hash);
  EXPECT_EQ(version, result_version);
}

TEST_F(BrowserUtilTest, RecordDataVer) {
  std::string user_id_hash = "1234";
  base::Version version{"1.1.1.1"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version);

  base::Value::Dict expected;
  expected.Set(user_id_hash, version.GetString());
  const base::Value::Dict& dict =
      pref_service_.GetDict(browser_util::kDataVerPref);
  EXPECT_EQ(dict, expected);
}

TEST_F(BrowserUtilTest, RecordDataVerOverrides) {
  std::string user_id_hash = "1234";

  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version1);
  browser_util::RecordDataVer(&pref_service_, user_id_hash, version2);

  base::Value::Dict expected;
  expected.Set(user_id_hash, version2.GetString());

  const base::Value::Dict& dict =
      pref_service_.GetDict(browser_util::kDataVerPref);
  EXPECT_EQ(dict, expected);
}

TEST_F(BrowserUtilTest, RecordDataVerWithMultipleUsers) {
  std::string user_id_hash_1 = "1234";
  std::string user_id_hash_2 = "2345";
  base::Version version1{"1.1.1.1"};
  base::Version version2{"1.1.1.2"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash_1, version1);
  browser_util::RecordDataVer(&pref_service_, user_id_hash_2, version2);

  EXPECT_EQ(version1, browser_util::GetDataVer(&pref_service_, user_id_hash_1));
  EXPECT_EQ(version2, browser_util::GetDataVer(&pref_service_, user_id_hash_2));

  base::Version version3{"3.3.3.3"};
  browser_util::RecordDataVer(&pref_service_, user_id_hash_1, version3);

  base::Value::Dict expected;
  expected.Set(user_id_hash_1, version3.GetString());
  expected.Set(user_id_hash_2, version2.GetString());

  const base::Value::Dict& dict =
      pref_service_.GetDict(browser_util::kDataVerPref);
  EXPECT_EQ(dict, expected);
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlock) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kVersion = "91.0.4457";
  const std::string kContent =
      "{\"content\":{\"version\":\"" + kVersion + "\"}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_EQ(browser_util::GetRootfsLacrosVersionMayBlock(path),
            base::Version(kVersion));
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingVersion) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{\"content\":{}}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingContent) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "{}";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockMissingFile) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  auto bad_path = tmp_dir.GetPath().Append("file");

  EXPECT_FALSE(
      browser_util::GetRootfsLacrosVersionMayBlock(bad_path).IsValid());
}

TEST_F(BrowserUtilTest, GetRootfsLacrosVersionMayBlockBadJson) {
  base::ScopedTempDir tmp_dir;
  ASSERT_TRUE(tmp_dir.CreateUniqueTempDir());
  const std::string kContent = "!@#$";
  auto path = tmp_dir.GetPath().Append("file");
  ASSERT_TRUE(base::WriteFile(path, kContent));

  EXPECT_FALSE(browser_util::GetRootfsLacrosVersionMayBlock(path).IsValid());
}

TEST_F(BrowserUtilTest, StatefulLacrosSelectionUpdateChannel) {
  // Assert that when no Lacros stability switch is specified, we return the
  // "unknown" channel.
  ASSERT_EQ(Channel::UNKNOWN, browser_util::GetLacrosSelectionUpdateChannel(
                                  LacrosSelection::kStateful));

  // Assert that when a Lacros stability switch is specified, we return the
  // relevant channel name associated to that switch value.
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  cmdline->AppendSwitchNative(browser_util::kLacrosStabilitySwitch,
                              browser_util::kLacrosStabilityChannelBeta);
  ASSERT_EQ(Channel::BETA, browser_util::GetLacrosSelectionUpdateChannel(
                               LacrosSelection::kStateful));
  cmdline->RemoveSwitch(browser_util::kLacrosStabilitySwitch);
}

TEST_F(BrowserUtilTest, GetMigrationStatus) {
  using ash::standalone_browser::migrator_util::MigrationMode;
  using browser_util::GetMigrationStatus;
  using browser_util::MigrationStatus;

  AddRegularUser("user@test.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);

  EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
            MigrationStatus::kLacrosNotEnabled);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});

  EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
            MigrationStatus::kUncompleted);

  {
    for (int i = 0;
         i < ash::standalone_browser::migrator_util::kMaxMigrationAttemptCount;
         i++) {
      ash::standalone_browser::migrator_util::
          UpdateMigrationAttemptCountForUser(&pref_service_,
                                             user->username_hash());
    }

    EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
              MigrationStatus::kMaxAttemptReached);

    ash::standalone_browser::migrator_util::ClearMigrationAttemptCountForUser(
        &pref_service_, user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        &pref_service_, user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kCopy);

    EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
              MigrationStatus::kCopyCompleted);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(&pref_service_,
                                              user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        &pref_service_, user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kMove);

    EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
              MigrationStatus::kMoveCompleted);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(&pref_service_,
                                              user->username_hash());
  }

  {
    ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
        &pref_service_, user->username_hash(),
        ash::standalone_browser::migrator_util::MigrationMode::kSkipForNewUser);

    EXPECT_EQ(GetMigrationStatus(&pref_service_, user),
              MigrationStatus::kSkippedForNewUser);

    ash::standalone_browser::migrator_util::
        ClearProfileMigrationCompletedForUser(&pref_service_,
                                              user->username_hash());
  }
}

TEST_F(BrowserUtilTest, IsAshBrowserSyncEnabled) {
  {
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});
    EXPECT_FALSE(browser_util::IsLacrosEnabled());
    EXPECT_TRUE(browser_util::IsAshWebBrowserEnabled());
    EXPECT_TRUE(browser_util::IsAshBrowserSyncEnabled());
  }
}

TEST_F(BrowserUtilTest, GetLacrosLaunchSwitchSourceNonGoogle) {
  AddRegularUser("user@random.com");

  // If LaunchSwitch is not set, the source is unknown.
  EXPECT_EQ(LacrosLaunchSwitchSource::kUnknown,
            browser_util::GetLacrosLaunchSwitchSource());

  // If the policy says UserChoice, lacros state may be set by user.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // The policy cannot be ignored by command line flag.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // Otherwise, the LaunchSwitch is set by the policy.
  for (const auto launch_switch : {LacrosAvailability::kLacrosDisallowed,
                                   LacrosAvailability::kLacrosOnly}) {
    ScopedLacrosAvailabilityCache cache(launch_switch);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByPolicy,
              browser_util::GetLacrosLaunchSwitchSource())
        << static_cast<int>(launch_switch);
  }
}

TEST_F(BrowserUtilTest, GetLacrosLaunchSwitchSourceGoogle) {
  AddRegularUser("user@google.com");

  // If LaunchSwitch is not set, the source is unknown.
  EXPECT_EQ(LacrosLaunchSwitchSource::kUnknown,
            browser_util::GetLacrosLaunchSwitchSource());

  // If the policy says UserChoice, lacros state may be set by user.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    EXPECT_EQ(LacrosLaunchSwitchSource::kPossiblySetByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }

  // The policy can be ignored by command line flag.
  {
    ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kLacrosAvailabilityIgnore);
    EXPECT_EQ(LacrosLaunchSwitchSource::kForcedByUser,
              browser_util::GetLacrosLaunchSwitchSource());
  }
}

// Lacros availability has no effect on non-googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreNonGoogle) {
  AddRegularUser("user@random.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
  EXPECT_FALSE(browser_util::IsLacrosAllowedToBeEnabled());
}

// Lacros availability has an effect on googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreGoogleDisableToUserChoice) {
  AddRegularUser("user@google.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosDisallowed);
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
}

// Lacros availability has an effect on googlers
TEST_F(BrowserUtilTest, LacrosAvailabilityIgnoreGoogleEnableToUserChoice) {
  AddRegularUser("user@google.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosAvailabilityIgnore);
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_FALSE(browser_util::IsLacrosEnabled());
}

// Check that the exist configurations used for the Google rollout have the
// precisely intended side-effects.
TEST_F(BrowserUtilTest, LacrosGoogleRolloutUserChoice) {
  AddRegularUser("user@google.com");
  // Lacros availability is set by policy to user choice.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kUserChoice);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
}

TEST_F(BrowserUtilTest, LacrosGoogleRolloutOnly) {
  AddRegularUser("user@google.com");
  const user_manager::User* const user =
      ash::ProfileHelper::Get()->GetUserByProfile(&testing_profile_);
  // Lacros availability is set by policy to only.
  ScopedLacrosAvailabilityCache cache(LacrosAvailability::kLacrosOnly);

  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});

  // Check that Lacros is allowed, enabled, and set to lacros-only.
  EXPECT_TRUE(browser_util::IsLacrosAllowedToBeEnabled());
  EXPECT_TRUE(browser_util::IsLacrosEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabled());
  EXPECT_FALSE(browser_util::IsAshWebBrowserEnabledForMigration(
      user, browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserUtilTest, IsArcAvailable) {
  arc::SetArcAvailableCommandLineForTesting(
      base::CommandLine::ForCurrentProcess());
  IdleServiceAsh::DisableForTesting();
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  AddRegularUser("user@google.com");

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);
  EXPECT_TRUE(browser_init_params->device_properties->is_arc_available);
  EXPECT_FALSE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(BrowserUtilTest, IsTabletFormFactor) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      ash::switches::kEnableTabletFormFactor);
  IdleServiceAsh::DisableForTesting();
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  AddRegularUser("user@google.com");

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);
  EXPECT_FALSE(browser_init_params->device_properties->is_arc_available);
  EXPECT_TRUE(browser_init_params->device_properties->is_tablet_form_factor);
}

TEST_F(BrowserUtilTest, SerialNumber) {
  IdleServiceAsh::DisableForTesting();
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  AddRegularUser("user@google.com");

  std::string expected_serial_number = "fake-serial-number";
  statistics_provider_.SetMachineStatistic("serial_number",
                                           expected_serial_number);

  EnvironmentProvider environment_provider;
  mojom::BrowserInitParamsPtr browser_init_params =
      browser_util::GetBrowserInitParams(
          &environment_provider,
          browser_util::InitialBrowserAction(
              crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow),
          /*is_keep_alive_enabled=*/false, absl::nullopt);

  auto serial_number = browser_init_params->device_properties->serial_number;
  ASSERT_TRUE(serial_number.has_value());
  EXPECT_EQ(serial_number.value(), expected_serial_number);
}

TEST_F(BrowserUtilTest, LacrosSelection) {
  // Neither policy nor command line have any preference on Lacros selection.
  EXPECT_FALSE(browser_util::DetermineLacrosSelection());

  {
    // LacrosSelection policy has precedence over command line.
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kRootfs);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        browser_util::kLacrosSelectionSwitch,
        browser_util::kLacrosSelectionStateful);
    EXPECT_EQ(browser_util::DetermineLacrosSelection(),
              LacrosSelection::kRootfs);
  }

  {
    // LacrosSelection allows command line check, but command line is not set.
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kUserChoice);
    EXPECT_FALSE(browser_util::DetermineLacrosSelection());
  }

  {
    // LacrosSelection allows command line check.
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        browser_util::kLacrosSelectionSwitch,
        browser_util::kLacrosSelectionRootfs);
    EXPECT_EQ(browser_util::DetermineLacrosSelection(),
              LacrosSelection::kRootfs);
  }

  {
    // LacrosSelection allows command line check.
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kUserChoice);
    base::test::ScopedCommandLine cmd_line;
    cmd_line.GetProcessCommandLine()->AppendSwitchASCII(
        browser_util::kLacrosSelectionSwitch,
        browser_util::kLacrosSelectionStateful);
    EXPECT_EQ(browser_util::DetermineLacrosSelection(),
              LacrosSelection::kStateful);
  }
}

// LacrosSelection has no effect on non-googlers.
TEST_F(BrowserUtilTest, LacrosSelectionPolicyIgnoreNonGoogle) {
  AddRegularUser("user@random.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosSelectionPolicyIgnore);

  {
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(browser_util::GetCachedLacrosSelectionPolicy(),
              browser_util::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(browser_util::DetermineLacrosSelection(),
              LacrosSelection::kRootfs);
  }
}

// LacrosSelection has an effect on googlers.
TEST_F(BrowserUtilTest, LacrosSelectionPolicyIgnoreGoogleDisableToUserChoice) {
  AddRegularUser("user@google.com");

  base::test::ScopedCommandLine cmd_line;
  cmd_line.GetProcessCommandLine()->AppendSwitch(
      ash::switches::kLacrosSelectionPolicyIgnore);

  {
    ScopedLacrosSelectionCache cache(
        browser_util::LacrosSelectionPolicy::kRootfs);
    EXPECT_EQ(browser_util::GetCachedLacrosSelectionPolicy(),
              browser_util::LacrosSelectionPolicy::kUserChoice);
    EXPECT_FALSE(browser_util::DetermineLacrosSelection());
  }
}

}  // namespace crosapi
