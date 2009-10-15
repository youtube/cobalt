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
int NumberOfFiles(const FilePath& path) {
  file_util::FileEnumerator iter(path, false, file_util::FileEnumerator::FILES);
  int count = 0;
  for (FilePath file = iter.Next(); !file.value().empty(); file = iter.Next()) {
    count++;
  }
  return count;
}

}  // namespace;

namespace disk_cache {

TEST_F(DiskCacheTest, BlockFiles_Grow) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[i]));
  }
  EXPECT_EQ(6, NumberOfFiles(path));

  // Make sure we don't keep adding files.
  for (int i = 0; i < kMaxSize * 4; i += 2) {
    int target = i % kMaxSize;
    files.DeleteBlock(address[target], false);
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[target]));
  }
  EXPECT_EQ(6, NumberOfFiles(path));
}

// We should be able to delete empty block files.
TEST_F(DiskCacheTest, BlockFiles_Shrink) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kMaxSize = 35000;
  Addr address[kMaxSize];

  // Fill up the 32-byte block file (use three files).
  for (int i = 0; i < kMaxSize; i++) {
    EXPECT_TRUE(files.CreateBlock(RANKINGS, 4, &address[i]));
  }

  // Now delete all the blocks, so that we can delete the two extra files.
  for (int i = 0; i < kMaxSize; i++) {
    files.DeleteBlock(address[i], false);
  }
  EXPECT_EQ(4, NumberOfFiles(path));
}

// Handling of block files not properly closed.
TEST_F(DiskCacheTest, BlockFiles_Recover) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  const int kNumEntries = 2000;
  CacheAddr entries[kNumEntries];

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);
  for (int i = 0; i < kNumEntries; i++) {
    Addr address(0);
    int size = (rand() % 4) + 1;
    EXPECT_TRUE(files.CreateBlock(RANKINGS, size, &address));
    entries[i] = address.value();
  }

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    CacheAddr temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    Addr address(entries[i]);
    files.DeleteBlock(address, false);
  }

  // At this point, there are kNumEntries / 2 entries on the file, randomly
  // distributed both on location and size.

  Addr address(entries[kNumEntries / 2]);
  MappedFile* file = files.GetFile(address);
  ASSERT_TRUE(NULL != file);

  BlockFileHeader* header =
      reinterpret_cast<BlockFileHeader*>(file->buffer());
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

  header = reinterpret_cast<BlockFileHeader*>(file->buffer());
  ASSERT_TRUE(NULL != header);

  ASSERT_EQ(0, header->updating);

  EXPECT_EQ(max_entries, header->max_entries);
  EXPECT_EQ(empty_1, header->empty[0]);
  EXPECT_EQ(empty_2, header->empty[1]);
  EXPECT_EQ(empty_3, header->empty[2]);
  EXPECT_EQ(empty_4, header->empty[3]);
}

// Handling of truncated files.
TEST_F(DiskCacheTest, BlockFiles_ZeroSizeFile) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  FilePath filename = files.Name(0);
  files.CloseFiles();
  // Truncate one of the files.
  {
    scoped_refptr<File> file(new File);
    ASSERT_TRUE(file->Init(filename));
    EXPECT_TRUE(file->SetLength(0));
  }

  // Initializing should fail, not crash.
  ASSERT_FALSE(files.Init(false));
}

// An invalid file can be detected after init.
TEST_F(DiskCacheTest, BlockFiles_InvalidFile) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  ASSERT_TRUE(file_util::CreateDirectory(path));

  BlockFiles files(path);
  ASSERT_TRUE(files.Init(true));

  // Let's access block 10 of file 5. (There is no file).
  Addr addr(BLOCK_256, 1, 5, 10);
  EXPECT_TRUE(NULL == files.GetFile(addr));

  // Let's create an invalid file.
  FilePath filename(files.Name(5));
  char header[kBlockHeaderSize];
  memset(header, 'a', kBlockHeaderSize);
  EXPECT_EQ(kBlockHeaderSize,
            file_util::WriteFile(filename, header, kBlockHeaderSize));

  EXPECT_TRUE(NULL == files.GetFile(addr));

  // The file should not have been cached (it is still invalid).
  EXPECT_TRUE(NULL == files.GetFile(addr));
}

}  // namespace disk_cache
