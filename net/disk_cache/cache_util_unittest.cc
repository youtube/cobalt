// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/cache_util.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace disk_cache {

class CacheUtilTest : public PlatformTest {
 public:
  void SetUp() {
    PlatformTest::SetUp();
    ASSERT_TRUE(tmp_dir_.CreateUniqueTempDir());
    cache_dir_ = tmp_dir_.path().Append(FILE_PATH_LITERAL("Cache"));
    file1_ = FilePath(cache_dir_.Append(FILE_PATH_LITERAL("file01")));
    file2_ = FilePath(cache_dir_.Append(FILE_PATH_LITERAL(".file02")));
    dir1_ = FilePath(cache_dir_.Append(FILE_PATH_LITERAL("dir01")));
    ASSERT_TRUE(file_util::CreateDirectory(cache_dir_));
    FILE *fp = file_util::OpenFile(file1_, "w");
    ASSERT_TRUE(fp != NULL);
    file_util::CloseFile(fp);
    fp = file_util::OpenFile(file2_, "w");
    ASSERT_TRUE(fp != NULL);
    file_util::CloseFile(fp);
    ASSERT_TRUE(file_util::CreateDirectory(dir1_));
    dest_dir_ = tmp_dir_.path().Append(FILE_PATH_LITERAL("old_Cache_001"));
    dest_file1_ = FilePath(dest_dir_.Append(FILE_PATH_LITERAL("file01")));
    dest_file2_ = FilePath(dest_dir_.Append(FILE_PATH_LITERAL(".file02")));
    dest_dir1_ = FilePath(dest_dir_.Append(FILE_PATH_LITERAL("dir01")));
  }

 protected:
  ScopedTempDir tmp_dir_;
  FilePath cache_dir_;
  FilePath file1_;
  FilePath file2_;
  FilePath dir1_;
  FilePath dest_dir_;
  FilePath dest_file1_;
  FilePath dest_file2_;
  FilePath dest_dir1_;
};

TEST_F(CacheUtilTest, MoveCache) {
  EXPECT_TRUE(disk_cache::MoveCache(cache_dir_, dest_dir_));
  EXPECT_TRUE(file_util::PathExists(dest_dir_));
  EXPECT_TRUE(file_util::PathExists(dest_file1_));
  EXPECT_TRUE(file_util::PathExists(dest_file2_));
  EXPECT_TRUE(file_util::PathExists(dest_dir1_));
#if defined(OS_CHROMEOS)
  EXPECT_TRUE(file_util::PathExists(cache_dir_)); // old cache dir stays
#else
  EXPECT_FALSE(file_util::PathExists(cache_dir_)); // old cache is gone
#endif
  EXPECT_FALSE(file_util::PathExists(file1_));
  EXPECT_FALSE(file_util::PathExists(file2_));
  EXPECT_FALSE(file_util::PathExists(dir1_));
}

TEST_F(CacheUtilTest, DeleteCache) {
  // DeleteCache won't delete subdirs, so let's not start with this
  // one around.
  file_util::Delete(dir1_, false);
  disk_cache::DeleteCache(cache_dir_, false);
  EXPECT_TRUE(file_util::PathExists(cache_dir_)); // cache dir stays
  EXPECT_FALSE(file_util::PathExists(file1_));
  EXPECT_FALSE(file_util::PathExists(file2_));
}

TEST_F(CacheUtilTest, DeleteCacheAndDir) {
  // DeleteCache won't delete subdirs, so let's not start with this
  // one around.
  file_util::Delete(dir1_, false);
  disk_cache::DeleteCache(cache_dir_, true);
  EXPECT_FALSE(file_util::PathExists(cache_dir_)); // cache dir is gone
  EXPECT_FALSE(file_util::PathExists(file1_));
  EXPECT_FALSE(file_util::PathExists(file2_));
}

TEST_F(CacheUtilTest, DeleteCacheFile) {
  EXPECT_TRUE(disk_cache::DeleteCacheFile(file1_));
  EXPECT_FALSE(file_util::PathExists(file1_));
  EXPECT_TRUE(file_util::PathExists(cache_dir_)); // cache dir stays
}

}  // namespace disk_cache
