// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface.

#ifndef NET_DISK_CACHE_BLOCK_FILES_H__
#define NET_DISK_CACHE_BLOCK_FILES_H__
#pragma once

#include <vector>

#include "base/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/addr.h"
#include "net/disk_cache/mapped_file.h"

namespace base {
class ThreadChecker;
}

namespace disk_cache {

// This class handles the set of block-files open by the disk cache.
class BlockFiles {
 public:
  explicit BlockFiles(const FilePath& path);
  ~BlockFiles();

  // Performs the object initialization. create_files indicates if the backing
  // files should be created or just open.
  bool Init(bool create_files);

  // Returns the file that stores a given address.
  MappedFile* GetFile(Addr address);

  // Creates a new entry on a block file. block_type indicates the size of block
  // to be used (as defined on cache_addr.h), block_count is the number of
  // blocks to allocate, and block_address is the address of the new entry.
  bool CreateBlock(FileType block_type, int block_count, Addr* block_address);

  // Removes an entry from the block files. If deep is true, the storage is zero
  // filled; otherwise the entry is removed but the data is not altered (must be
  // already zeroed).
  void DeleteBlock(Addr address, bool deep);

  // Close all the files and set the internal state to be initializad again. The
  // cache is being purged.
  void CloseFiles();

  // Sends UMA stats.
  void ReportStats();

  // Returns true if the blocks pointed by a given address are currently used.
  // This method is only intended for debugging.
  bool IsValid(Addr address);

 private:
  // Set force to true to overwrite the file if it exists.
  bool CreateBlockFile(int index, FileType file_type, bool force);
  bool OpenBlockFile(int index);

  // Attemp to grow this file. Fails if the file cannot be extended anymore.
  bool GrowBlockFile(MappedFile* file, BlockFileHeader* header);

  // Returns the appropriate file to use for a new block.
  MappedFile* FileForNewBlock(FileType block_type, int block_count);

  // Returns the next block file on this chain, creating new files if needed.
  MappedFile* NextFile(const MappedFile* file);

  // Creates an empty block file and returns its index.
  int CreateNextBlockFile(FileType block_type);

  // Removes a chained block file that is now empty.
  void RemoveEmptyFile(FileType block_type);

  // Restores the header of a potentially inconsistent file.
  bool FixBlockFileHeader(MappedFile* file);

  // Retrieves stats for the given file index.
  void GetFileStats(int index, int* used_count, int* load);

  // Returns the filename for a given file index.
  FilePath Name(int index);

  bool init_;
  char* zero_buffer_;  // Buffer to speed-up cleaning deleted entries.
  FilePath path_;  // Path to the backing folder.
  std::vector<MappedFile*> block_files_;  // The actual files.
  scoped_ptr<base::ThreadChecker> thread_checker_;

  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_ZeroSizeFile);
  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_InvalidFile);
  FRIEND_TEST_ALL_PREFIXES(DiskCacheTest, BlockFiles_Stats);

  DISALLOW_COPY_AND_ASSIGN(BlockFiles);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCK_FILES_H__
