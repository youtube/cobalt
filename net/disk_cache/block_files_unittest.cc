// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

// Returns the number of files in this folder.
int NumberOfFiles(const std::wstring path) {
  file_util::FileEnumerator iter(FilePath::FromWStringHack(path), false,
                                 file_util::FileEnumerator::FILES);
  int count = 0;
  for (FilePath file = iter.Next(); !file.value().empty(); file = iter.Next()) {
    count++;
  }
  return count;
}

}  // namespace;

TEST_F(DiskCacheTest, BlockFiles_Grow) {
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  disk_cache::Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, 4, &address[i]));
  }
  EXPECT_EQ(6, NumberOfFiles(path));

  // Make sure we don't keep adding files.
  for (int i = 0; i < kMaxSize * 4; i += 2) {
    int target = i % kMaxSize;
    files.DeleteBlock(address[target], false);
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, 4, &address[target]));
  }
  EXPECT_EQ(6, NumberOfFiles(path));
}

// We should be able to delete empty block files.
TEST_F(DiskCacheTest, BlockFiles_Shrink) {
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  disk_cache::Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, 4, &address[i]));
  }

  // Now delete all the blocks, so that we can delete the two extra files.
  for (int i = 0; i < kMaxSize; i++) {
    files.DeleteBlock(address[i], false);
  }
  EXPECT_EQ(4, NumberOfFiles(path));
}

// Handling of block files not properly closed.
TEST_F(DiskCacheTest, BlockFiles_Recover) {
  std::wstring path = GetCachePath();
  ASSERT_TRUE(DeleteCache(path.c_str()));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  disk_cache::BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kNumEntries = 2000;
  disk_cache::CacheAddr entries[kNumEntries];

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  for (int i = 0; i < kNumEntries; i++) {
    disk_cache::Addr address(0);
    int size = (rand() % 4) + 1;
    EXPECT_TRUE(files.CreateBlock(disk_cache::RANKINGS, size, &address));
    entries[i] = address.value();
  }

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    disk_cache::CacheAddr temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    disk_cache::Addr address(entries[i]);
    files.DeleteBlock(address, false);
  }

  // At this point, there are kNumEntries / 2 entries on the file, randomly
  // distributed both on location and size.

  disk_cache::Addr address(entries[kNumEntries / 2]);
  disk_cache::MappedFile* file = files.GetFile(address);
  ASSERT_TRUE(NULL != file);

  disk_cache::BlockFileHeader* header =
      reinterpret_cast<disk_cache::BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(NULL != header);

  ASSERT_EQ(0, header->updating);

  int max_entries = header->max_entries;
  int empty_1 = header->empty[0];
  int empty_2 = header->empty[1];
  int empty_3 = header->empty[2];
  int empty_4 = header->empty[3];

  // Corrupt the file.
  header->max_entries = header->empty[0] = 0;
  header->empty[1] = header->empty[2] = header->empty[3] = 0;
  header->updating = -1;

  files.CloseFiles();

  ASSERT_TRUE(files.Init(false));

  // The file must have been fixed.
  file = files.GetFile(address);
  ASSERT_TRUE(NULL != file);

  header = reinterpret_cast<disk_cache::BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(NULL != header);

  ASSERT_EQ(0, header->updating);

  EXPECT_EQ(max_entries, header->max_entries);
  EXPECT_EQ(empty_1, header->empty[0]);
  EXPECT_EQ(empty_2, header->empty[1]);
  EXPECT_EQ(empty_3, header->empty[2]);
  EXPECT_EQ(empty_4, header->empty[3]);
}
