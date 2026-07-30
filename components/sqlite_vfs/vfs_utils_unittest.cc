// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sqlite_vfs/vfs_utils.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/gmock_expected_support.h"
#include "build/build_config.h"
#include "components/sqlite_vfs/client.h"
#include "components/sqlite_vfs/constants.h"
#include "components/sqlite_vfs/file_set_error.h"
#include "components/sqlite_vfs/pending_file_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sqlite_vfs {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;

class VfsUtilsTest : public testing::Test {
 protected:
  static constexpr base::FilePath::StringViewType kBaseName =
      FILE_PATH_LITERAL("TEST");

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  const base::FilePath& temp_dir_path() const { return temp_dir_.GetPath(); }

  base::ScopedTempDir temp_dir_;
};

TEST_F(VfsUtilsTest, MakePendingFileSetSuccess) {
  EXPECT_THAT(MakePendingFileSet(Client::kTest, temp_dir_path(),
                                 base::FilePath(kBaseName),
                                 /*single_connection=*/false,
                                 /*journal_mode_wal=*/false),
              HasValue());
}

TEST_F(VfsUtilsTest, MakePendingFileSetDirectoryNotFound) {
  base::FilePath non_existent_dir =
      temp_dir_path().Append(FILE_PATH_LITERAL("non_existent_subdir"));
  EXPECT_THAT(MakePendingFileSet(Client::kTest, non_existent_dir,
                                 base::FilePath(kBaseName),
                                 /*single_connection=*/false,
                                 /*journal_mode_wal=*/false),
              ErrorIs(FileSetError::kPermanent));
}

#if BUILDFLAG(IS_WIN)
TEST_F(VfsUtilsTest, MakePendingFileSetInUseWin) {
  base::FilePath db_path =
      temp_dir_path().Append(kBaseName).AddExtension(kDbFileExtension);

  // Open the file exclusively to simulate in-use.
  base::File exclusive_file(db_path, base::File::FLAG_CREATE_ALWAYS |
                                         base::File::FLAG_WRITE |
                                         base::File::FLAG_WIN_EXCLUSIVE_READ |
                                         base::File::FLAG_WIN_EXCLUSIVE_WRITE);
  ASSERT_TRUE(exclusive_file.IsValid());

  // Try to create a pending file set with single_connection=true (which
  // requests exclusive access).
  EXPECT_THAT(MakePendingFileSet(Client::kTest, temp_dir_path(),
                                 base::FilePath(kBaseName),
                                 /*single_connection=*/true,
                                 /*journal_mode_wal=*/false),
              ErrorIs(FileSetError::kTransient));
}
#endif

TEST_F(VfsUtilsTest, MakePendingFileSetNotAFile) {
  base::FilePath db_path =
      temp_dir_path().Append(kBaseName).AddExtension(kDbFileExtension);

  // Create a directory where the DB file should be.
  ASSERT_TRUE(base::CreateDirectory(db_path));

  EXPECT_THAT(MakePendingFileSet(Client::kTest, temp_dir_path(),
                                 base::FilePath(kBaseName),
                                 /*single_connection=*/false,
                                 /*journal_mode_wal=*/false),
              ErrorIs(FileSetError::kPermanentRequireDeletion));
}

}  // namespace

}  // namespace sqlite_vfs
