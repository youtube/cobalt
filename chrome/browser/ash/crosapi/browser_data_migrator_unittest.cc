// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_migrator.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/crosapi/fake_migration_progress_tracker.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/standalone_browser/migrator_util.h"
#include "chromeos/ash/components/standalone_browser/standalone_browser_features.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/version_info/version_info.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {
constexpr char kFirstRun[] = "First Run";
}  // namespace

class BrowserDataMigratorImplTest : public ::testing::Test {
 public:
  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // ./                             /* user_data_dir_ */
    // |- user/                       /* from_dir_ */
    //     |- Preferences

    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    from_dir_ = user_data_dir_.GetPath().Append("user");

    ASSERT_TRUE(base::CreateDirectory(from_dir_));
    ASSERT_TRUE(
        base::WriteFile(from_dir_.Append(chrome::kPreferencesFilename), "{}"));

    BrowserDataMigratorImpl::RegisterLocalStatePrefs(pref_service_.registry());
    crosapi::browser_util::RegisterLocalStatePrefs(pref_service_.registry());
    ash::standalone_browser::migrator_util::RegisterLocalStatePrefs(
        pref_service_.registry());
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  base::ScopedTempDir user_data_dir_;
  base::FilePath from_dir_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(BrowserDataMigratorImplTest, Migrate) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  ash::standalone_browser::migrator_util::UpdateMigrationAttemptCountForUser(
      &pref_service_, user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  run_loop.Run();

  const base::FilePath new_user_data_dir =
      from_dir_.Append(browser_data_migrator_util::kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  // Check that `First Run` file is created inside the new data directory.
  EXPECT_TRUE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  // Check that migration is marked as completed for the user.
  EXPECT_TRUE(
      ash::standalone_browser::migrator_util::
          IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kSucceeded, result->kind);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // Successful migration should clear the migration attempt count.
  EXPECT_EQ(
      ash::standalone_browser::migrator_util::GetMigrationAttemptCountForUser(
          &pref_service_, user_id_hash),
      0);
  // Data version should be updated to the current version after a migration.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}

TEST_F(BrowserDataMigratorImplTest, MigrateCancelled) {
  base::test::TaskEnvironment task_environment;
  std::unique_ptr<MigrationProgressTracker> progress_tracker =
      std::make_unique<FakeMigrationProgressTracker>();
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  // Set migration attempt count to 1.
  ash::standalone_browser::migrator_util::UpdateMigrationAttemptCountForUser(
      &pref_service_, user_id_hash);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  migrator->Cancel();
  run_loop.Run();

  const base::FilePath new_user_data_dir =
      from_dir_.Append(browser_data_migrator_util::kLacrosDir);
  const base::FilePath new_profile_data_dir =
      new_user_data_dir.Append("Default");
  EXPECT_FALSE(base::PathExists(new_user_data_dir.Append(kFirstRun)));
  EXPECT_FALSE(
      ash::standalone_browser::migrator_util::
          IsProfileMigrationCompletedForUser(&pref_service_, user_id_hash));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kCancelled, result->kind);
  EXPECT_EQ(BrowserDataMigratorImpl::GetMigrationStep(&pref_service_),
            BrowserDataMigratorImpl::MigrationStep::kEnded);
  // If migration fails, migration attempt count should not be cleared thus
  // should remain as 1.
  EXPECT_EQ(
      ash::standalone_browser::migrator_util::GetMigrationAttemptCountForUser(
          &pref_service_, user_id_hash),
      1);
  // Even if migration is cancelled, lacros data dir is cleared and thus data
  // version should be updated.
  EXPECT_EQ(crosapi::browser_util::GetDataVer(&pref_service_, user_id_hash),
            version_info::GetVersion());
}

TEST_F(BrowserDataMigratorImplTest, MigrateOutOfDisk) {
  // Emulate the situation of out-of-disk.
  browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
      scoped_extra_bytes(100);

  base::test::TaskEnvironment task_environment;
  const std::string user_id_hash = "abcd";
  BrowserDataMigratorImpl::SetMigrationStep(
      &pref_service_, BrowserDataMigratorImpl::MigrationStep::kRestartCalled);

  base::RunLoop run_loop;
  std::unique_ptr<BrowserDataMigratorImpl> migrator =
      std::make_unique<BrowserDataMigratorImpl>(
          from_dir_, user_id_hash, base::DoNothing(), &pref_service_);
  absl::optional<BrowserDataMigrator::Result> result;
  migrator->Migrate(base::BindLambdaForTesting(
      [&out_result = result, &run_loop](BrowserDataMigrator::Result result) {
        run_loop.Quit();
        out_result = result;
      }));
  run_loop.Run();

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(BrowserDataMigrator::ResultKind::kFailed, result->kind);
  // |required_size| should carry the data.
  EXPECT_EQ(100u, result->required_size);
}

class BrowserDataMigratorRestartTest : public ::testing::Test {
 public:
  BrowserDataMigratorRestartTest() = default;
  ~BrowserDataMigratorRestartTest() override = default;

  void SetUp() override {
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
  }

 protected:
  ash::FakeChromeUserManager* user_manager() {
    return fake_user_manager_.Get();
  }
  PrefService* local_state() { return fake_user_manager_->GetLocalState(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
  FakeSessionManagerClient session_manager_;
};

TEST_F(BrowserDataMigratorRestartTest, MaybeRestartToMigrateWithMigrationStep) {
  bool restart_called = false;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting(
          [&restart_called]() { restart_called = true; }));

  BrowserDataMigratorImpl::SetMigrationStep(
      local_state(), BrowserDataMigratorImpl::MigrationStep::kRestartCalled);
  EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrate(
      AccountId::FromUserEmail("fake@gmail.com"), "abcde",
      crosapi::browser_util::PolicyInitState::kAfterInit));

  BrowserDataMigratorImpl::SetMigrationStep(
      local_state(), BrowserDataMigratorImpl::MigrationStep::kStarted);
  EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrate(
      AccountId::FromUserEmail("fake@gmail.com"), "abcde",
      crosapi::browser_util::PolicyInitState::kAfterInit));

  BrowserDataMigratorImpl::SetMigrationStep(
      local_state(), BrowserDataMigratorImpl::MigrationStep::kEnded);
  EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrate(
      AccountId::FromUserEmail("fake@gmail.com"), "abcde",
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataMigratorRestartTest, MaybeRestartToMigrateWithCommandLine) {
  bool restart_called = false;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting(
          [&restart_called]() { restart_called = true; }));

  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@gmail.com");
  const user_manager::User* const user = user_manager()->GetPrimaryUser();
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataMigrationForTesting, "force-skip");
    EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrate(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataMigrationForTesting, "force-migration");
    EXPECT_TRUE(BrowserDataMigratorImpl::MaybeRestartToMigrate(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserDataMigratorRestartTest, MaybeRestartToMigrateWithDiskCheck) {
  bool restart_called = false;
  ScopedRestartAttemptForTesting scoped_restart_attempt(
      base::BindLambdaForTesting(
          [&restart_called]() { restart_called = true; }));

  base::test::ScopedFeatureList feature_list;
  AddRegularUser("user@gmail.com");
  const user_manager::User* const user = user_manager()->GetPrimaryUser();
  // If MaybeRestartToMigrate will skip the restarting, WithDiskCheck variation
  // also skips it.
  {
    restart_called = false;
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataMigrationForTesting, "force-skip");
    absl::optional<bool> result;
    BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheck(
        user->GetAccountId(), user->username_hash(),
        base::BindLambdaForTesting(
            [&out_result = result](bool result,
                                   const absl::optional<uint64_t>& size) {
              out_result = result;
            }));
    EXPECT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
    EXPECT_FALSE(restart_called);
  }

  // If MaybeRestartToMigrate will trigger the restarting, WithDiskCheck
  // variation will see additional disk size check.
  {
    restart_called = false;
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataMigrationForTesting, "force-migration");
    // Inject the behavior that the disk does not have enough space.
    browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
        scoped_extra_bytes(1024 * 1024);

    absl::optional<bool> result;
    absl::optional<uint64_t> out_size;
    base::RunLoop run_loop;
    BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheck(
        user->GetAccountId(), user->username_hash(),
        base::BindLambdaForTesting(
            [&out_result = result, &out_size, &run_loop](
                bool result, const absl::optional<uint64_t>& size) {
              run_loop.Quit();
              out_result = result;
              out_size = size;
            }));
    run_loop.Run();
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value());
    EXPECT_EQ(1024u * 1024u, out_size);
    EXPECT_FALSE(restart_called);
  }

  {
    restart_called = false;
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataMigrationForTesting, "force-migration");
    // Inject the behavior that the disk has enough space for the migration.
    browser_data_migrator_util::ScopedExtraBytesRequiredToBeFreedForTesting
        scoped_extra_bytes(0);

    absl::optional<bool> result;
    base::RunLoop run_loop;
    BrowserDataMigratorImpl::MaybeRestartToMigrateWithDiskCheck(
        user->GetAccountId(), user->username_hash(),
        base::BindLambdaForTesting(
            [&out_result = result, &run_loop](
                bool result, const absl::optional<uint64_t>& size) {
              run_loop.Quit();
              out_result = result;
            }));
    run_loop.Run();
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value());
    EXPECT_TRUE(restart_called);
  }
}

TEST_F(BrowserDataMigratorRestartTest, MaybeRestartToMigrateMoveAfterCopy) {
  // Check that `MaybeRestartToMigrateInternal()` returns false after completion
  // of copy migration even if move migration is enabled.
  AddRegularUser("user@gmail.com");
  const user_manager::User* const user = user_manager()->GetPrimaryUser();

  {
    // If Lacros is not enabled, migration should not run.
    base::test::ScopedFeatureList feature_list;
    EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }

  {
    // If Lacros is enabled, migration should run.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});
    EXPECT_TRUE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }

  // Mark copy migration as completed.
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kCopy);
  {
    // If copy migration is marked as completed then migration should not run
    // even if move migration is not completed.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});
    EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }

  // Mark move migration as completed.
  ash::standalone_browser::migrator_util::ClearProfileMigrationCompletedForUser(
      local_state(), user->username_hash());
  ash::standalone_browser::migrator_util::SetProfileMigrationCompletedForUser(
      local_state(), user->username_hash(),
      ash::standalone_browser::migrator_util::MigrationMode::kMove);
  {
    // If move migration is marked as completed, move migration should not run.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});
    EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserDataMigratorRestartTest, MaybeRestartToMigrateSecondaryUser) {
  // Add two users to simulate multi user session.
  AddRegularUser("user1@gmail.com");
  AddRegularUser("user2@gmail.com");
  const auto* const primary_user = user_manager()->GetPrimaryUser();
  const auto* const secondary_user =
      user_manager()->FindUser(AccountId::FromUserEmail("user2@gmail.com"));
  EXPECT_NE(primary_user, secondary_user);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::standalone_browser::features::kLacrosOnly}, {});
    // Migration should be triggered for the primary user.
    EXPECT_TRUE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        primary_user->GetAccountId(), primary_user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
    // But not for secondary users.
    EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
        secondary_user->GetAccountId(), secondary_user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserDataMigratorRestartTest,
       MaybeRestartToMigrateWithMaximumRetryAttempts) {
  // Check that `MaybeRestartToMigrateInternal()` returns false if maximum retry
  // attempts have been reached.
  AddRegularUser("user@gmail.com");
  const user_manager::User* const user = user_manager()->GetPrimaryUser();

  // If Lacros is enabled, migration should run.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {ash::standalone_browser::features::kLacrosOnly}, {});
  EXPECT_TRUE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
      user->GetAccountId(), user->username_hash(),
      crosapi::browser_util::PolicyInitState::kAfterInit));

  for (int i = 0;
       i < ash::standalone_browser::migrator_util::kMaxMigrationAttemptCount;
       i++) {
    ash::standalone_browser::migrator_util::UpdateMigrationAttemptCountForUser(
        local_state(), user->username_hash());
  }
  // If maximum attempts have been reached then profile migration should be
  // skipped.
  EXPECT_FALSE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
      user->GetAccountId(), user->username_hash(),
      crosapi::browser_util::PolicyInitState::kAfterInit));

  ash::standalone_browser::migrator_util::ClearMigrationAttemptCountForUser(
      local_state(), user->username_hash());
  EXPECT_TRUE(BrowserDataMigratorImpl::MaybeRestartToMigrateInternal(
      user->GetAccountId(), user->username_hash(),
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

}  // namespace ash
