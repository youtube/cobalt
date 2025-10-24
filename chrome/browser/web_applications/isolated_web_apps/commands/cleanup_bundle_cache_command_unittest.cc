// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/commands/cleanup_bundle_cache_command.h"

#include "ash/constants/ash_paths.h"
#include "base/files/file_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_storage_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/test_signed_web_bundle_builder.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

using base::test::ErrorIs;
using base::test::TestFuture;
using base::test::ValueIs;
using web_package::SignedWebBundleId;
using CleanupResult = CleanupBundleCacheResult;
using Callback = base::OnceCallback<void(CleanupResult)>;
using SessionType = IwaCacheClient::SessionType;

const SignedWebBundleId kMainBundleId = test::GetDefaultEd25519WebBundleId();
const SignedWebBundleId kBundleId2 = test::GetDefaultEcdsaP256WebBundleId();
const base::Version kVersion = base::Version("0.0.1");

}  // namespace

class CleanupBundleCacheCommandTest
    : public WebAppTest,
      public testing::WithParamInterface<SessionType> {
 public:
  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    ASSERT_TRUE(cache_root_dir_.CreateUniqueTempDir());
    cache_root_dir_override_ = std::make_unique<base::ScopedPathOverride>(
        ash::DIR_DEVICE_LOCAL_ACCOUNT_IWA_CACHE, cache_root_dir_.GetPath());
  }

  const base::FilePath& CacheRootPath() { return cache_root_dir_.GetPath(); }

  base::FilePath CreateBundleInCacheDir(const SignedWebBundleId& bundle_id,
                                        const base::Version& version) {
    return CreateBundleInCacheDirForSession(bundle_id, version,
                                            GetSessionType());
  }

  base::FilePath CreateBundleInCacheDirForSession(
      const SignedWebBundleId& bundle_id,
      const base::Version& version,
      SessionType session_type) {
    base::FilePath bundle_directory_path =
        GetBundleDirWithVersion(bundle_id, version, session_type);
    EXPECT_TRUE(base::CreateDirectory(bundle_directory_path));

    base::FilePath temp_file;
    EXPECT_TRUE(base::CreateTemporaryFileInDir(CacheRootPath(), &temp_file));

    base::FilePath bundle_path =
        IwaCacheClient::GetBundleFullName(bundle_directory_path);
    EXPECT_TRUE(base::CopyFile(temp_file, bundle_path));
    return bundle_path;
  }

  base::FilePath GetBundleDir(const SignedWebBundleId& bundle_id) {
    return GetBundleDirForSession(bundle_id, GetSessionType());
  }

  base::FilePath GetBundleDirForSession(const SignedWebBundleId& bundle_id,
                                        SessionType session_type) {
    return IwaCacheClient::GetCacheDirectoryForBundle(
        IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type,
                                                            CacheRootPath()),
        bundle_id);
  }

  base::FilePath GetBundleDirWithVersion(const SignedWebBundleId& bundle_id,
                                         const base::Version& version,
                                         SessionType session_type) {
    auto session_cache_dir =
        IwaCacheClient::GetCacheBaseDirectoryForSessionType(session_type,
                                                            CacheRootPath());
    return IwaCacheClient::GetCacheDirectoryForBundleWithVersion(
        session_cache_dir, bundle_id, version);
  }

  void ScheduleCommand(
      const std::vector<web_package::SignedWebBundleId>& iwas_to_keep_in_cache,
      Callback callback) {
    fake_provider().scheduler().CleanupIsolatedWebAppBundleCache(
        iwas_to_keep_in_cache, GetSessionType(), std::move(callback));
  }

  void RestrictDirectoryPermission(const base::FilePath& dir) {
    // Allows to check that file exists, but disallows to change it.
    EXPECT_TRUE(
        SetPosixFilePermissions(dir, base::FILE_PERMISSION_EXECUTE_BY_USER));
  }

  SessionType GetSessionType() { return GetParam(); }

 private:
  base::ScopedTempDir cache_root_dir_;
  std::unique_ptr<base::ScopedPathOverride> cache_root_dir_override_;
};

TEST_P(CleanupBundleCacheCommandTest, NoBundles) {
  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {}, cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/0}));
}

TEST_P(CleanupBundleCacheCommandTest, KeepTheOnlyApp) {
  const base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, kVersion);

  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {kMainBundleId},
                  cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/0}));
  EXPECT_TRUE(base::PathExists(bundle_path));
}

TEST_P(CleanupBundleCacheCommandTest, KeepTwoApps) {
  const base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion);
  const base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId2, kVersion);

  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {kMainBundleId, kBundleId2},
                  cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/0}));
  EXPECT_TRUE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
}

TEST_P(CleanupBundleCacheCommandTest, RemoveTheOnlyApp) {
  const base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, kVersion);

  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {}, cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/1}));
  EXPECT_FALSE(base::PathExists(bundle_path));
}

TEST_P(CleanupBundleCacheCommandTest, RemoveCorrectBundle) {
  const base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion);
  const base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId2, kVersion);

  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {kBundleId2},
                  cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/1}));
  EXPECT_FALSE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
}

TEST_P(CleanupBundleCacheCommandTest, IwaNotCached) {
  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {kMainBundleId},
                  cleanup_future.GetCallback());

  // `kMainBundleId` is not cached, but it still should finish with success.
  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/0}));
}

TEST_P(CleanupBundleCacheCommandTest, FailedToDeleteOneDir) {
  const base::FilePath bundle_path =
      CreateBundleInCacheDir(kMainBundleId, kVersion);
  const base::FilePath bundle_dir = GetBundleDir(kMainBundleId);

  // `CleanupBundleCacheCommand` tries to delete IWA directory, but can't do it
  // because it does not have write permissions.
  RestrictDirectoryPermission(bundle_dir);
  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {}, cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ErrorIs(CleanupBundleCacheError{
                  CleanupBundleCacheError::Type::kCouldNotDeleteAllBundles,
                  /*number_of_failed_to_cleaned_up_directories=*/1}));
  EXPECT_TRUE(base::PathExists(bundle_path));
}

TEST_P(CleanupBundleCacheCommandTest, FailedToDeleteMultipleDirs) {
  const base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion);
  const base::FilePath bundle_dir1 = GetBundleDir(kMainBundleId);
  const base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId2, kVersion);
  const base::FilePath bundle_dir2 = GetBundleDir(kBundleId2);

  // `CleanupBundleCacheCommand` tries to delete IWA directories, but can't do
  // it because it does not have write permissions.
  RestrictDirectoryPermission(bundle_dir1);
  RestrictDirectoryPermission(bundle_dir2);
  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {}, cleanup_future.GetCallback());

  ASSERT_FALSE(cleanup_future.Get().has_value());
  EXPECT_THAT(cleanup_future.Get().error().type(),
              CleanupBundleCacheError::Type::kCouldNotDeleteAllBundles);
  EXPECT_THAT(
      cleanup_future.Get().error().number_of_failed_to_cleaned_up_directories(),
      2);
  EXPECT_TRUE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
}

TEST_P(CleanupBundleCacheCommandTest, PartiallyFailedToDeleteDirs) {
  const base::FilePath bundle_path1 =
      CreateBundleInCacheDir(kMainBundleId, kVersion);
  const base::FilePath bundle_dir1 = GetBundleDir(kMainBundleId);
  const base::FilePath bundle_path2 =
      CreateBundleInCacheDir(kBundleId2, kVersion);
  const base::FilePath bundle_dir2 = GetBundleDir(kBundleId2);

  // `CleanupBundleCacheCommand` tries to delete IWA directory, but it can
  // delete only one directory.
  RestrictDirectoryPermission(bundle_dir2);
  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {}, cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ErrorIs(CleanupBundleCacheError{
                  CleanupBundleCacheError::Type::kCouldNotDeleteAllBundles,
                  /*number_of_failed_to_cleaned_up_directories=*/1}));
  EXPECT_FALSE(base::PathExists(bundle_path1));
  EXPECT_TRUE(base::PathExists(bundle_path2));
}

TEST_P(CleanupBundleCacheCommandTest, RemoveBundleForCorrectSession) {
  const base::FilePath bundle_path_kiosk1 = CreateBundleInCacheDirForSession(
      kMainBundleId, kVersion, SessionType::kKiosk);
  const base::FilePath bundle_path_kiosk2 = CreateBundleInCacheDirForSession(
      kBundleId2, kVersion, SessionType::kKiosk);
  const base::FilePath bundle_path_mgs1 = CreateBundleInCacheDirForSession(
      kMainBundleId, kVersion, SessionType::kManagedGuestSession);
  const base::FilePath bundle_path_mgs2 = CreateBundleInCacheDirForSession(
      kBundleId2, kVersion, SessionType::kManagedGuestSession);

  TestFuture<CleanupResult> cleanup_future;
  ScheduleCommand(/*iwas_to_keep_in_cache*/ {kBundleId2},
                  cleanup_future.GetCallback());

  EXPECT_THAT(cleanup_future.Get(),
              ValueIs(CleanupBundleCacheSuccess{
                  /*number_of_cleaned_up_directories=*/1}));

  switch (GetSessionType()) {
    case IwaCacheClient::SessionType::kKiosk:
      EXPECT_FALSE(base::PathExists(bundle_path_kiosk1));
      EXPECT_TRUE(base::PathExists(bundle_path_kiosk2));
      EXPECT_TRUE(base::PathExists(bundle_path_mgs1));
      EXPECT_TRUE(base::PathExists(bundle_path_mgs2));
      break;
    case IwaCacheClient::SessionType::kManagedGuestSession:
      EXPECT_TRUE(base::PathExists(bundle_path_kiosk1));
      EXPECT_TRUE(base::PathExists(bundle_path_kiosk2));
      EXPECT_FALSE(base::PathExists(bundle_path_mgs1));
      EXPECT_TRUE(base::PathExists(bundle_path_mgs2));
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    CleanupBundleCacheCommandTest,
    testing::Values(SessionType::kKiosk, SessionType::kManagedGuestSession));

}  // namespace web_app
