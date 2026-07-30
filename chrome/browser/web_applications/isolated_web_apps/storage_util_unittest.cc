// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/storage_util.h"

#include <string>
#include <utility>
#include <variant>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/to_string.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::base::test::HasValue;

struct VerifyRelocationVisitor {
  explicit VerifyRelocationVisitor(
      base::FilePath profile_dir,
      base::FilePath source_path,
      IwaSourceBundleModeAndFileOp bundle_mode_and_file_op)
      : profile_dir_(std::move(profile_dir)),
        source_path_(std::move(source_path)),
        bundle_mode_and_file_op_(bundle_mode_and_file_op) {}

  void operator()(const IwaStorageOwnedBundle& location) {
    // Owned bundles should be relocated to the profile's IWA directory.
    base::FilePath path = location.GetPath(profile_dir_);
    EXPECT_TRUE(base::PathExists(path));
    EXPECT_EQ(location.dir_name_ascii().length(), 16u);
    EXPECT_THAT(location.dir_name_ascii(),
                ::testing::MatchesRegex("[a-z2-7]{16}"));
    switch (bundle_mode_and_file_op_) {
      case IwaSourceBundleModeAndFileOp::kDevModeCopy:
      case IwaSourceBundleModeAndFileOp::kProdModeCopy:
        EXPECT_TRUE(base::PathExists(source_path_));
        break;
      case IwaSourceBundleModeAndFileOp::kDevModeMove:
      case IwaSourceBundleModeAndFileOp::kProdModeMove:
        EXPECT_FALSE(base::PathExists(source_path_));
        break;
    }
    EXPECT_NE(path, source_path_);
    EXPECT_EQ(path.DirName().DirName(), profile_dir_.Append(kIwaDirName));
    EXPECT_EQ(path.BaseName(), base::FilePath(kMainSwbnFileName));
  }

  void operator()(const IwaStorageUnownedBundle& location) { FAIL(); }

  void operator()(const IwaStorageProxy& location) { FAIL(); }

 private:
  base::FilePath profile_dir_;
  base::FilePath source_path_;
  IwaSourceBundleModeAndFileOp bundle_mode_and_file_op_;
};

struct VerifyCleanupVisitor {
  explicit VerifyCleanupVisitor(base::FilePath profile_dir)
      : profile_dir_(std::move(profile_dir)) {}

  void operator()(const IwaStorageOwnedBundle& location) {
    // Owned bundles should be cleaned up, including their parent directory.
    base::FilePath path = location.GetPath(profile_dir_);
    EXPECT_FALSE(base::PathExists(path));
    EXPECT_FALSE(base::PathExists(path.DirName()));
  }

  void operator()(const IwaStorageUnownedBundle& location) {
    // Unowned bundles should not be cleaned up.
    EXPECT_TRUE(base::PathExists(location.path()));
  }

  void operator()(const IwaStorageProxy& location) { FAIL(); }

 private:
  base::FilePath profile_dir_;
};

class IwaStorageUtilRelocationTest
    : public ::testing::TestWithParam<IwaSourceBundleModeAndFileOp> {
 public:
  using RelocationResult =
      base::expected<IsolatedWebAppStorageLocation, std::string>;

  void SetUp() override {
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir_));

    // A directory where source files are stored.
    ASSERT_TRUE(base::CreateTemporaryDirInDir(
        temp_dir.GetPath(), FILE_PATH_LITERAL("src"), &src_dir_));
  }

 protected:
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;

  base::FilePath profile_dir_;
  base::FilePath src_dir_;
};

TEST_P(IwaStorageUtilRelocationTest, NormalFlow) {
  base::FilePath bundle;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(src_dir_, &bundle));

  IwaSourceWithModeAndFileOp source{
      IwaSourceBundleWithModeAndFileOp(bundle, GetParam())};

  // Check that relocation works.
  base::test::TestFuture<RelocationResult> future;
  UpdateBundlePathAndCreateStorageLocation(profile_dir_, source,
                                           future.GetCallback());
  RelocationResult result = future.Take();
  EXPECT_THAT(result, HasValue());
  std::visit(VerifyRelocationVisitor{profile_dir_, bundle, GetParam()},
             result->variant());

  // Check that cleanup works.
  base::test::TestFuture<void> cleanup_future;
  CleanupLocationIfOwned(profile_dir_, result.value(),
                         cleanup_future.GetCallback());
  ASSERT_TRUE(cleanup_future.Wait());
  std::visit(VerifyCleanupVisitor{profile_dir_}, result->variant());
}

TEST_P(IwaStorageUtilRelocationTest, ErrorFlowSourceMissing) {
  base::FilePath bundle = src_dir_.AppendASCII("missing.swbn");

  IwaSourceWithModeAndFileOp source{
      IwaSourceBundleWithModeAndFileOp(bundle, GetParam())};

  base::test::TestFuture<RelocationResult> future;
  UpdateBundlePathAndCreateStorageLocation(profile_dir_, source,
                                           future.GetCallback());
  RelocationResult result = future.Take();
  EXPECT_THAT(result, ::testing::Not(HasValue()));

  base::FilePath iwa_root_dir = profile_dir_.Append(kIwaDirName);
  if (base::DirectoryExists(iwa_root_dir)) {
    EXPECT_TRUE(base::IsDirectoryEmpty(iwa_root_dir));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaStorageUtilRelocationTest,
    ::testing::Values(IwaSourceBundleModeAndFileOp::kDevModeCopy,
                      IwaSourceBundleModeAndFileOp::kDevModeMove,
                      IwaSourceBundleModeAndFileOp::kProdModeCopy,
                      IwaSourceBundleModeAndFileOp::kProdModeMove),
    [](const testing::TestParamInfo<IwaStorageUtilRelocationTest::ParamType>&
           info) { return base::ToString(info.param); });

TEST(IwaStorageUtilCleanupTest, CleanupNotOwned) {
  base::test::TaskEnvironment task_environment;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath profile_dir;
  ASSERT_TRUE(base::CreateTemporaryDirInDir(
      temp_dir.GetPath(), FILE_PATH_LITERAL("profile"), &profile_dir));

  // Create a file that is not in the owned IWA directory.
  base::FilePath bundle_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(temp_dir.GetPath(), &bundle_path));

  // Trying to cleanup the location that is not owned.
  IwaStorageUnownedBundle location{bundle_path};
  base::test::TestFuture<void> cleanup_future;
  CleanupLocationIfOwned(profile_dir, location, cleanup_future.GetCallback());
  ASSERT_TRUE(cleanup_future.Wait());

  // Not owned file should not be deleted.
  EXPECT_TRUE(base::PathExists(bundle_path));
}

}  // namespace
}  // namespace web_app
