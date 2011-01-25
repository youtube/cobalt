// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_
#define NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_
#pragma once

#include "base/basictypes.h"
#include "base/threading/thread.h"
#include "net/base/cache_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class FilePath;

namespace net {

class IOBuffer;

}  // namespace net

namespace disk_cache {

class Backend;
class BackendImpl;
class Entry;
class MemBackendImpl;

}  // namespace disk_cache

// These tests can use the path service, which uses autoreleased objects on the
// Mac, so this needs to be a PlatformTest.  Even tests that do not require a
// cache (and that do not need to be a DiskCacheTestWithCache) are susceptible
// to this problem; all such tests should use TEST_F(DiskCacheTest, ...).
class DiskCacheTest : public PlatformTest {
  virtual void TearDown();
};

// Provides basic support for cache related tests.
class DiskCacheTestWithCache : public DiskCacheTest {
 protected:
  DiskCacheTestWithCache();
  virtual ~DiskCacheTestWithCache();

  void InitCache();
  void SimulateCrash();
  void SetTestMode();

  void SetMemoryOnlyMode() {
    memory_only_ = true;
  }

  // Use the implementation directly instead of the factory provided object.
  void SetDirectMode() {
    implementation_ = true;
  }

  void SetMask(uint32 mask) {
    mask_ = mask;
  }

  void SetMaxSize(int size);

  // Deletes and re-creates the files on initialization errors.
  void SetForceCreation() {
    force_creation_ = true;
  }

  void SetNewEviction() {
    new_eviction_ = true;
  }

  void DisableFirstCleanup() {
    first_cleanup_ = false;
  }

  void DisableIntegrityCheck() {
    integrity_ = false;
  }

  void UseCurrentThread() {
    use_current_thread_ = true;
  }

  void SetCacheType(net::CacheType type) {
    type_ = type;
  }

  // Utility methods to access the cache and wait for each operation to finish.
  int OpenEntry(const std::string& key, disk_cache::Entry** entry);
  int CreateEntry(const std::string& key, disk_cache::Entry** entry);
  int DoomEntry(const std::string& key);
  int DoomAllEntries();
  int DoomEntriesBetween(const base::Time initial_time,
                         const base::Time end_time);
  int DoomEntriesSince(const base::Time initial_time);
  int OpenNextEntry(void** iter, disk_cache::Entry** next_entry);
  void FlushQueueForTest();
  void RunTaskForTest(Task* task);
  int ReadData(disk_cache::Entry* entry, int index, int offset,
               net::IOBuffer* buf, int len);
  int WriteData(disk_cache::Entry* entry, int index, int offset,
                net::IOBuffer* buf, int len, bool truncate);
  int ReadSparseData(disk_cache::Entry* entry, int64 offset, net::IOBuffer* buf,
                     int len);
  int WriteSparseData(disk_cache::Entry* entry, int64 offset,
                      net::IOBuffer* buf, int len);

  // Asks the cache to trim a an entry. If |empty| is true, the whole entry is
  // deleted.
  void TrimForTest(bool empty);

  // Asks the cache to trim a an entry from the deleted list. If |empty| is
  // true, the whole entry is deleted.
  void TrimDeletedListForTest(bool empty);

  // DiskCacheTest:
  virtual void TearDown();

  // cache_ will always have a valid object, regardless of how the cache was
  // initialized. The implementation pointers can be NULL.
  disk_cache::Backend* cache_;
  disk_cache::BackendImpl* cache_impl_;
  disk_cache::MemBackendImpl* mem_cache_;

  uint32 mask_;
  int size_;
  net::CacheType type_;
  bool memory_only_;
  bool implementation_;
  bool force_creation_;
  bool new_eviction_;
  bool first_cleanup_;
  bool integrity_;
  bool use_current_thread_;
  // This is intentionally left uninitialized, to be used by any test.
  bool success_;

 private:
  void InitMemoryCache();
  void InitDiskCache();
  void InitDiskCacheImpl(const FilePath& path);

  base::Thread cache_thread_;
  DISALLOW_COPY_AND_ASSIGN(DiskCacheTestWithCache);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_BASE_H_
