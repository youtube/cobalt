// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/third_party/dynamic_annotations/dynamic_annotations.h"
#include "base/threading/platform_thread.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache_test_base.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/histogram_macros.h"
#include "net/disk_cache/mapped_file.h"
#include "net/disk_cache/mem_backend_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

using base::Time;

// Tests that can run with different types of caches.
class DiskCacheBackendTest : public DiskCacheTestWithCache {
 protected:
  void BackendBasics();
  void BackendKeying();
  void BackendSetSize();
  void BackendLoad();
  void BackendValidEntry();
  void BackendInvalidEntry();
  void BackendInvalidEntryRead();
  void BackendInvalidEntryWithLoad();
  void BackendTrimInvalidEntry();
  void BackendTrimInvalidEntry2();
  void BackendEnumerations();
  void BackendEnumerations2();
  void BackendInvalidEntryEnumeration();
  void BackendFixEnumerators();
  void BackendDoomRecent();
  void BackendDoomBetween();
  void BackendTransaction(const std::string& name, int num_entries, bool load);
  void BackendRecoverInsert();
  void BackendRecoverRemove();
  void BackendRecoverWithEviction();
  void BackendInvalidEntry2();
  void BackendInvalidEntry3();
  void BackendNotMarkedButDirty(const std::string& name);
  void BackendDoomAll();
  void BackendDoomAll2();
  void BackendInvalidRankings();
  void BackendInvalidRankings2();
  void BackendDisable();
  void BackendDisable2();
  void BackendDisable3();
  void BackendDisable4();
};

void DiskCacheBackendTest::BackendBasics() {
  InitCache();
  disk_cache::Entry *entry1 = NULL, *entry2 = NULL;
  EXPECT_NE(net::OK, OpenEntry("the first key", &entry1));
  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  EXPECT_NE(net::OK, CreateEntry("the first key", &entry1));
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry1));
  EXPECT_NE(net::OK, OpenEntry("some other key", &entry2));
  ASSERT_EQ(net::OK, CreateEntry("some other key", &entry2));
  ASSERT_TRUE(NULL != entry1);
  ASSERT_TRUE(NULL != entry2);
  EXPECT_EQ(2, cache_->GetEntryCount());

  disk_cache::Entry* entry3 = NULL;
  ASSERT_EQ(net::OK, OpenEntry("some other key", &entry3));
  ASSERT_TRUE(NULL != entry3);
  EXPECT_TRUE(entry2 == entry3);
  EXPECT_EQ(2, cache_->GetEntryCount());

  EXPECT_EQ(net::OK, DoomEntry("some other key"));
  EXPECT_EQ(1, cache_->GetEntryCount());
  entry1->Close();
  entry2->Close();
  entry3->Close();

  EXPECT_EQ(net::OK, DoomEntry("the first key"));
  EXPECT_EQ(0, cache_->GetEntryCount());

  ASSERT_EQ(net::OK, CreateEntry("the first key", &entry1));
  ASSERT_EQ(net::OK, CreateEntry("some other key", &entry2));
  entry1->Doom();
  entry1->Close();
  EXPECT_EQ(net::OK, DoomEntry("some other key"));
  EXPECT_EQ(0, cache_->GetEntryCount());
  entry2->Close();
}

TEST_F(DiskCacheBackendTest, Basics) {
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, NewEvictionBasics) {
  SetNewEviction();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyBasics) {
  SetMemoryOnlyMode();
  BackendBasics();
}

TEST_F(DiskCacheBackendTest, AppCacheBasics) {
  SetCacheType(net::APP_CACHE);
  BackendBasics();
}

void DiskCacheBackendTest::BackendKeying() {
  InitCache();
  const char* kName1 = "the first key";
  const char* kName2 = "the first Key";
  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, CreateEntry(kName1, &entry1));

  ASSERT_EQ(net::OK, CreateEntry(kName2, &entry2));
  EXPECT_TRUE(entry1 != entry2) << "Case sensitive";
  entry2->Close();

  char buffer[30];
  base::strlcpy(buffer, kName1, arraysize(buffer));
  ASSERT_EQ(net::OK, OpenEntry(buffer, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 1, kName1, arraysize(buffer) - 1);
  ASSERT_EQ(net::OK, OpenEntry(buffer + 1, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  base::strlcpy(buffer + 3,  kName1, arraysize(buffer) - 3);
  ASSERT_EQ(net::OK, OpenEntry(buffer + 3, &entry2));
  EXPECT_TRUE(entry1 == entry2);
  entry2->Close();

  // Now verify long keys.
  char buffer2[20000];
  memset(buffer2, 's', sizeof(buffer2));
  buffer2[1023] = '\0';
  ASSERT_EQ(net::OK, CreateEntry(buffer2, &entry2)) << "key on block file";
  entry2->Close();

  buffer2[1023] = 'g';
  buffer2[19999] = '\0';
  ASSERT_EQ(net::OK, CreateEntry(buffer2, &entry2)) << "key on external file";
  entry2->Close();
  entry1->Close();
}

TEST_F(DiskCacheBackendTest, Keying) {
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, NewEvictionKeying) {
  SetNewEviction();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyKeying) {
  SetMemoryOnlyMode();
  BackendKeying();
}

TEST_F(DiskCacheBackendTest, AppCacheKeying) {
  SetCacheType(net::APP_CACHE);
  BackendKeying();
}

TEST_F(DiskCacheTest, CreateBackend) {
  TestCompletionCallback cb;

  {
    FilePath path = GetCacheFilePath();
    ASSERT_TRUE(DeleteCache(path));
    base::Thread cache_thread("CacheThread");
    ASSERT_TRUE(cache_thread.StartWithOptions(
                    base::Thread::Options(MessageLoop::TYPE_IO, 0)));

    // Test the private factory methods.
    disk_cache::Backend* cache = NULL;
    int rv = disk_cache::BackendImpl::CreateBackend(
                 path, false, 0, net::DISK_CACHE, disk_cache::kNoRandom,
                 cache_thread.message_loop_proxy(), NULL, &cache, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));
    ASSERT_TRUE(cache);
    delete cache;

    cache = disk_cache::MemBackendImpl::CreateBackend(0);
    ASSERT_TRUE(cache);
    delete cache;
    cache = NULL;

    // Now test the public API.
    rv = disk_cache::CreateCacheBackend(net::DISK_CACHE, path, 0, false,
                                        cache_thread.message_loop_proxy(),
                                        NULL, &cache, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));
    ASSERT_TRUE(cache);
    delete cache;
    cache = NULL;

    rv = disk_cache::CreateCacheBackend(net::MEMORY_CACHE, FilePath(), 0, false,
                                        NULL, NULL, &cache, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));
    ASSERT_TRUE(cache);
    delete cache;
  }

  MessageLoop::current()->RunAllPending();
}

TEST_F(DiskCacheBackendTest, ExternalFiles) {
  InitCache();
  // First, let's create a file on the folder.
  FilePath filename = GetCacheFilePath().AppendASCII("f_000001");

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer1->data(), kSize, false);
  ASSERT_EQ(kSize, file_util::WriteFile(filename, buffer1->data(), kSize));

  // Now let's create a file with the cache.
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry("key", &entry));
  ASSERT_EQ(0, WriteData(entry, 0, 20000, buffer1, 0, false));
  entry->Close();

  // And verify that the first file is still there.
  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize));
  ASSERT_EQ(kSize, file_util::ReadFile(filename, buffer2->data(), kSize));
  EXPECT_EQ(0, memcmp(buffer1->data(), buffer2->data(), kSize));
}

// Tests that we deal with file-level pending operations at destruction time.
TEST_F(DiskCacheTest, ShutdownWithPendingIO) {
  TestCompletionCallback cb;

  {
    FilePath path = GetCacheFilePath();
    ASSERT_TRUE(DeleteCache(path));
    base::Thread cache_thread("CacheThread");
    ASSERT_TRUE(cache_thread.StartWithOptions(
                    base::Thread::Options(MessageLoop::TYPE_IO, 0)));

    disk_cache::Backend* cache;
    int rv = disk_cache::BackendImpl::CreateBackend(
                 path, false, 0, net::DISK_CACHE, disk_cache::kNoRandom,
                 base::MessageLoopProxy::CreateForCurrentThread(), NULL,
                 &cache, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));

    disk_cache::EntryImpl* entry;
    rv = cache->CreateEntry("some key",
                            reinterpret_cast<disk_cache::Entry**>(&entry), &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));

    const int kSize = 25000;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
    CacheTestFillBuffer(buffer->data(), kSize, false);

    for (int i = 0; i < 10 * 1024 * 1024; i += 64 * 1024) {
      // We are using the current thread as the cache thread because we want to
      // be able to call directly this method to make sure that the OS (instead
      // of us switching thread) is returning IO pending.
      rv = entry->WriteDataImpl(0, i, buffer, kSize, &cb, false);
      if (rv == net::ERR_IO_PENDING)
        break;
      EXPECT_EQ(kSize, rv);
    }

    // Don't call Close() to avoid going through the queue or we'll deadlock
    // waiting for the operation to finish.
    entry->Release();

    // The cache destructor will see one pending operation here.
    delete cache;

    if (rv == net::ERR_IO_PENDING) {
      EXPECT_TRUE(cb.have_result());
    }
  }

  MessageLoop::current()->RunAllPending();
}

// Tests that we deal with background-thread pending operations.
TEST_F(DiskCacheTest, ShutdownWithPendingIO2) {
  TestCompletionCallback cb;

  {
    FilePath path = GetCacheFilePath();
    ASSERT_TRUE(DeleteCache(path));
    base::Thread cache_thread("CacheThread");
    ASSERT_TRUE(cache_thread.StartWithOptions(
                    base::Thread::Options(MessageLoop::TYPE_IO, 0)));

    disk_cache::Backend* cache;
    int rv = disk_cache::BackendImpl::CreateBackend(
                 path, false, 0, net::DISK_CACHE, disk_cache::kNoRandom,
                 cache_thread.message_loop_proxy(), NULL, &cache, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));

    disk_cache::Entry* entry;
    rv = cache->CreateEntry("some key", &entry, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));

    const int kSize = 25000;
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
    CacheTestFillBuffer(buffer->data(), kSize, false);

    rv = entry->WriteData(0, 0, buffer, kSize, &cb, false);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    entry->Close();

    // The cache destructor will see two pending operations here.
    delete cache;
  }

  MessageLoop::current()->RunAllPending();
}

TEST_F(DiskCacheTest, TruncatedIndex) {
  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  FilePath index = path.AppendASCII("index");
  ASSERT_EQ(5, file_util::WriteFile(index, "hello", 5));

  base::Thread cache_thread("CacheThread");
  ASSERT_TRUE(cache_thread.StartWithOptions(
                  base::Thread::Options(MessageLoop::TYPE_IO, 0)));
  TestCompletionCallback cb;

  disk_cache::Backend* backend = NULL;
  int rv = disk_cache::BackendImpl::CreateBackend(
               path, false, 0, net::DISK_CACHE, disk_cache::kNone,
               cache_thread.message_loop_proxy(), NULL, &backend, &cb);
  ASSERT_NE(net::OK, cb.GetResult(rv));

  ASSERT_TRUE(backend == NULL);
  delete backend;
}

void DiskCacheBackendTest::BackendSetSize() {
  SetDirectMode();
  const int cache_size = 0x10000;  // 64 kB
  SetMaxSize(cache_size);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(first, &entry));

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(cache_size));
  memset(buffer->data(), 0, cache_size);
  EXPECT_EQ(cache_size / 10, WriteData(entry, 0, 0, buffer, cache_size / 10,
                                       false)) << "normal file";

  EXPECT_EQ(net::ERR_FAILED, WriteData(entry, 1, 0, buffer, cache_size / 5,
                                       false)) << "file size above the limit";

  // By doubling the total size, we make this file cacheable.
  SetMaxSize(cache_size * 2);
  EXPECT_EQ(cache_size / 5, WriteData(entry, 1, 0, buffer, cache_size / 5,
                                      false));

  // Let's fill up the cache!.
  SetMaxSize(cache_size * 10);
  EXPECT_EQ(cache_size * 3 / 4, WriteData(entry, 0, 0, buffer,
                                          cache_size * 3 / 4, false));
  entry->Close();
  FlushQueueForTest();

  SetMaxSize(cache_size);

  // The cache is 95% full.

  ASSERT_EQ(net::OK, CreateEntry(second, &entry));
  EXPECT_EQ(cache_size / 10, WriteData(entry, 0, 0, buffer, cache_size / 10,
                                       false));

  disk_cache::Entry* entry2;
  ASSERT_EQ(net::OK, CreateEntry("an extra key", &entry2));
  EXPECT_EQ(cache_size / 10, WriteData(entry2, 0, 0, buffer, cache_size / 10,
                                       false));
  entry2->Close();  // This will trigger the cache trim.

  EXPECT_NE(net::OK, OpenEntry(first, &entry2));

  FlushQueueForTest();  // Make sure that we are done trimming the cache.
  FlushQueueForTest();  // We may have posted two tasks to evict stuff.

  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(second, &entry));
  EXPECT_EQ(cache_size / 10, entry->GetDataSize(0));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, SetSize) {
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, NewEvictionSetSize) {
  SetNewEviction();
  BackendSetSize();
}

TEST_F(DiskCacheBackendTest, MemoryOnlySetSize) {
  SetMemoryOnlyMode();
  BackendSetSize();
}

void DiskCacheBackendTest::BackendLoad() {
  InitCache();
  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  disk_cache::Entry* entries[100];
  for (int i = 0; i < 100; i++) {
    std::string key = GenerateKey(true);
    ASSERT_EQ(net::OK, CreateEntry(key, &entries[i]));
  }
  EXPECT_EQ(100, cache_->GetEntryCount());

  for (int i = 0; i < 100; i++) {
    int source1 = rand() % 100;
    int source2 = rand() % 100;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  for (int i = 0; i < 100; i++) {
    disk_cache::Entry* entry;
    ASSERT_EQ(net::OK, OpenEntry(entries[i]->GetKey(), &entry));
    EXPECT_TRUE(entry == entries[i]);
    entry->Close();
    entries[i]->Doom();
    entries[i]->Close();
  }
  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, Load) {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, NewEvictionLoad) {
  SetNewEviction();
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyLoad) {
  // Work with a tiny index table (16 entries)
  SetMaxSize(0x100000);
  SetMemoryOnlyMode();
  BackendLoad();
}

TEST_F(DiskCacheBackendTest, AppCacheLoad) {
  SetCacheType(net::APP_CACHE);
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  BackendLoad();
}

// Before looking for invalid entries, let's check a valid entry.
void DiskCacheBackendTest::BackendValidEntry() {
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer1, kSize, false));
  entry->Close();
  SimulateCrash();

  ASSERT_EQ(net::OK, OpenEntry(key, &entry));

  scoped_refptr<net::IOBuffer> buffer2(new net::IOBuffer(kSize));
  memset(buffer2->data(), 0, kSize);
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer2, kSize));
  entry->Close();
  EXPECT_STREQ(buffer1->data(), buffer2->data());
}

TEST_F(DiskCacheBackendTest, ValidEntry) {
  BackendValidEntry();
}

TEST_F(DiskCacheBackendTest, NewEvictionValidEntry) {
  SetNewEviction();
  BackendValidEntry();
}

// The same logic of the previous test (ValidEntry), but this time force the
// entry to be invalid, simulating a crash in the middle.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntry() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  base::strlcpy(buffer->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));
  SimulateCrash();

  EXPECT_NE(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(0, cache_->GetEntryCount());
}

// This and the other intentionally leaky tests below are excluded from
// purify and valgrind runs by naming them in the files
//   net/data/purify/net_unittests.exe.gtest.txt and
//   net/data/valgrind/net_unittests.gtest.txt
// The scripts tools/{purify,valgrind}/chrome_tests.sh
// read those files and pass the appropriate --gtest_filter to net_unittests.
TEST_F(DiskCacheBackendTest, InvalidEntry) {
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry) {
  SetNewEviction();
  BackendInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntry) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntry();
}

// Almost the same test, but this time crash the cache after reading an entry.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryRead() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  base::strlcpy(buffer->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  EXPECT_EQ(kSize, ReadData(entry, 0, 0, buffer, kSize));

  SimulateCrash();

  if (type_ == net::APP_CACHE) {
    // Reading an entry and crashing should not make it dirty.
    ASSERT_EQ(net::OK, OpenEntry(key, &entry));
    EXPECT_EQ(1, cache_->GetEntryCount());
    entry->Close();
  } else {
    EXPECT_NE(net::OK, OpenEntry(key, &entry));
    EXPECT_EQ(0, cache_->GetEntryCount());
  }
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryRead) {
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryRead) {
  SetNewEviction();
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntryRead) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntryRead();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryWithLoad() {
  // Work with a tiny index table (16 entries)
  SetMask(0xf);
  SetMaxSize(0x100000);
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 100;
  disk_cache::Entry* entries[kNumEntries];
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    ASSERT_EQ(net::OK, CreateEntry(key, &entries[i]));
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  for (int i = 0; i < kNumEntries; i++) {
    int source1 = rand() % kNumEntries;
    int source2 = rand() % kNumEntries;
    disk_cache::Entry* temp = entries[source1];
    entries[source1] = entries[source2];
    entries[source2] = temp;
  }

  std::string keys[kNumEntries];
  for (int i = 0; i < kNumEntries; i++) {
    keys[i] = entries[i]->GetKey();
    if (i < kNumEntries / 2)
      entries[i]->Close();
  }

  SimulateCrash();

  for (int i = kNumEntries / 2; i < kNumEntries; i++) {
    disk_cache::Entry* entry;
    EXPECT_NE(net::OK, OpenEntry(keys[i], &entry));
  }

  for (int i = 0; i < kNumEntries / 2; i++) {
    disk_cache::Entry* entry;
    EXPECT_EQ(net::OK, OpenEntry(keys[i], &entry));
    entry->Close();
  }

  EXPECT_EQ(kNumEntries / 2, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryWithLoad) {
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryWithLoad) {
  SetNewEviction();
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, AppCacheInvalidEntryWithLoad) {
  SetCacheType(net::APP_CACHE);
  BackendInvalidEntryWithLoad();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendTrimInvalidEntry() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();

  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 10);
  InitCache();

  std::string first("some key");
  std::string second("something else");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(first, &entry));

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));

  // Simulate a crash.
  SimulateCrash();

  ASSERT_EQ(net::OK, CreateEntry(second, &entry));
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));

  EXPECT_EQ(2, cache_->GetEntryCount());
  SetMaxSize(kSize);
  entry->Close();  // Trim the cache.
  FlushQueueForTest();

  // If we evicted the entry in less than 20mS, we have one entry in the cache;
  // if it took more than that, we posted a task and we'll delete the second
  // entry too.
  MessageLoop::current()->RunAllPending();

  // This may be not thread-safe in general, but for now it's OK so add some
  // ThreadSanitizer annotations to ignore data races on cache_.
  // See http://crbug.com/55970
  ANNOTATE_IGNORE_READS_BEGIN();
  EXPECT_GE(1, cache_->GetEntryCount());
  ANNOTATE_IGNORE_READS_END();

  EXPECT_NE(net::OK, OpenEntry(first, &entry));
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, TrimInvalidEntry) {
  BackendTrimInvalidEntry();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry) {
  SetNewEviction();
  BackendTrimInvalidEntry();
}

// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendTrimInvalidEntry2() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  SetMask(0xf);  // 16-entry table.

  const int kSize = 0x3000;  // 12 kB
  SetMaxSize(kSize * 40);
  InitCache();

  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  memset(buffer->data(), 0, kSize);
  disk_cache::Entry* entry;

  // Writing 32 entries to this cache chains most of them.
  for (int i = 0; i < 32; i++) {
    std::string key(base::StringPrintf("some key %d", i));
    ASSERT_EQ(net::OK, CreateEntry(key, &entry));
    EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));
    entry->Close();
    ASSERT_EQ(net::OK, OpenEntry(key, &entry));
    // Note that we are not closing the entries.
  }

  // Simulate a crash.
  SimulateCrash();

  ASSERT_EQ(net::OK, CreateEntry("Something else", &entry));
  EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, false));

  EXPECT_EQ(33, cache_->GetEntryCount());
  SetMaxSize(kSize);

  // For the new eviction code, all corrupt entries are on the second list so
  // they are not going away that easy.
  if (new_eviction_) {
    EXPECT_EQ(net::OK, DoomAllEntries());
  }

  entry->Close();  // Trim the cache.
  FlushQueueForTest();

  // We may abort the eviction before cleaning up everything.
  MessageLoop::current()->RunAllPending();
  EXPECT_GE(30, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, TrimInvalidEntry2) {
  BackendTrimInvalidEntry2();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionTrimInvalidEntry2) {
  SetNewEviction();
  BackendTrimInvalidEntry2();
}

void DiskCacheBackendTest::BackendEnumerations() {
  InitCache();
  Time initial = Time::Now();
  int seed = static_cast<int>(initial.ToInternalValue());
  srand(seed);

  const int kNumEntries = 100;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_EQ(net::OK, CreateEntry(key, &entry));
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());
  Time final = Time::Now();

  disk_cache::Entry* entry;
  void* iter = NULL;
  int count = 0;
  Time last_modified[kNumEntries];
  Time last_used[kNumEntries];
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(NULL != entry);
    if (count < kNumEntries) {
      last_modified[count] = entry->GetLastModified();
      last_used[count] = entry->GetLastUsed();
      EXPECT_TRUE(initial <= last_modified[count]);
      EXPECT_TRUE(final >= last_modified[count]);
    }

    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);

  iter = NULL;
  count = 0;
  // The previous enumeration should not have changed the timestamps.
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(NULL != entry);
    if (count < kNumEntries) {
      EXPECT_TRUE(last_modified[count] == entry->GetLastModified());
      EXPECT_TRUE(last_used[count] == entry->GetLastUsed());
    }
    entry->Close();
    count++;
  };
  EXPECT_EQ(kNumEntries, count);
}

TEST_F(DiskCacheBackendTest, Enumerations) {
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, NewEvictionEnumerations) {
  SetNewEviction();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyEnumerations) {
  SetMemoryOnlyMode();
  BackendEnumerations();
}

TEST_F(DiskCacheBackendTest, AppCacheEnumerations) {
  SetCacheType(net::APP_CACHE);
  BackendEnumerations();
}

// Verifies enumerations while entries are open.
void DiskCacheBackendTest::BackendEnumerations2() {
  InitCache();
  const std::string first("first");
  const std::string second("second");
  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, CreateEntry(first, &entry1));
  entry1->Close();
  ASSERT_EQ(net::OK, CreateEntry(second, &entry2));
  entry2->Close();

  // Make sure that the timestamp is not the same.
  base::PlatformThread::Sleep(20);
  ASSERT_EQ(net::OK, OpenEntry(second, &entry1));
  void* iter = NULL;
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry2));
  EXPECT_EQ(entry2->GetKey(), second);

  // Two entries and the iterator pointing at "first".
  entry1->Close();
  entry2->Close();

  // The iterator should still be valid, so we should not crash.
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry2));
  EXPECT_EQ(entry2->GetKey(), first);
  entry2->Close();
  cache_->EndEnumeration(&iter);

  // Modify the oldest entry and get the newest element.
  ASSERT_EQ(net::OK, OpenEntry(first, &entry1));
  EXPECT_EQ(0, WriteData(entry1, 0, 200, NULL, 0, false));
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry2));
  if (type_ == net::APP_CACHE) {
    // The list is not updated.
    EXPECT_EQ(entry2->GetKey(), second);
  } else {
    EXPECT_EQ(entry2->GetKey(), first);
  }

  entry1->Close();
  entry2->Close();
  cache_->EndEnumeration(&iter);
}

TEST_F(DiskCacheBackendTest, Enumerations2) {
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, NewEvictionEnumerations2) {
  SetNewEviction();
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyEnumerations2) {
  SetMemoryOnlyMode();
  BackendEnumerations2();
}

TEST_F(DiskCacheBackendTest, AppCacheEnumerations2) {
  SetCacheType(net::APP_CACHE);
  BackendEnumerations2();
}

// Verify handling of invalid entries while doing enumerations.
// We'll be leaking memory from this test.
void DiskCacheBackendTest::BackendInvalidEntryEnumeration() {
  // Use the implementation directly... we need to simulate a crash.
  SetDirectMode();
  InitCache();

  std::string key("Some key");
  disk_cache::Entry *entry, *entry1, *entry2;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry1));

  const int kSize = 50;
  scoped_refptr<net::IOBuffer> buffer1(new net::IOBuffer(kSize));
  memset(buffer1->data(), 0, kSize);
  base::strlcpy(buffer1->data(), "And the data to save", kSize);
  EXPECT_EQ(kSize, WriteData(entry1, 0, 0, buffer1, kSize, false));
  entry1->Close();
  ASSERT_EQ(net::OK, OpenEntry(key, &entry1));
  EXPECT_EQ(kSize, ReadData(entry1, 0, 0, buffer1, kSize));

  std::string key2("Another key");
  ASSERT_EQ(net::OK, CreateEntry(key2, &entry2));
  entry2->Close();
  ASSERT_EQ(2, cache_->GetEntryCount());

  SimulateCrash();

  void* iter = NULL;
  int count = 0;
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(NULL != entry);
    EXPECT_EQ(key2, entry->GetKey());
    entry->Close();
    count++;
  };
  EXPECT_EQ(1, count);
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, InvalidEntryEnumeration) {
  BackendInvalidEntryEnumeration();
}

// We'll be leaking memory from this test.
TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntryEnumeration) {
  SetNewEviction();
  BackendInvalidEntryEnumeration();
}

// Tests that if for some reason entries are modified close to existing cache
// iterators, we don't generate fatal errors or reset the cache.
void DiskCacheBackendTest::BackendFixEnumerators() {
  InitCache();

  int seed = static_cast<int>(Time::Now().ToInternalValue());
  srand(seed);

  const int kNumEntries = 10;
  for (int i = 0; i < kNumEntries; i++) {
    std::string key = GenerateKey(true);
    disk_cache::Entry* entry;
    ASSERT_EQ(net::OK, CreateEntry(key, &entry));
    entry->Close();
  }
  EXPECT_EQ(kNumEntries, cache_->GetEntryCount());

  disk_cache::Entry *entry1, *entry2;
  void* iter1 = NULL;
  void* iter2 = NULL;
  ASSERT_EQ(net::OK, OpenNextEntry(&iter1, &entry1));
  ASSERT_TRUE(NULL != entry1);
  entry1->Close();
  entry1 = NULL;

  // Let's go to the middle of the list.
  for (int i = 0; i < kNumEntries / 2; i++) {
    if (entry1)
      entry1->Close();
    ASSERT_EQ(net::OK, OpenNextEntry(&iter1, &entry1));
    ASSERT_TRUE(NULL != entry1);

    ASSERT_EQ(net::OK, OpenNextEntry(&iter2, &entry2));
    ASSERT_TRUE(NULL != entry2);
    entry2->Close();
  }

  // Messing up with entry1 will modify entry2->next.
  entry1->Doom();
  ASSERT_EQ(net::OK, OpenNextEntry(&iter2, &entry2));
  ASSERT_TRUE(NULL != entry2);

  // The link entry2->entry1 should be broken.
  EXPECT_NE(entry2->GetKey(), entry1->GetKey());
  entry1->Close();
  entry2->Close();

  // And the second iterator should keep working.
  ASSERT_EQ(net::OK, OpenNextEntry(&iter2, &entry2));
  ASSERT_TRUE(NULL != entry2);
  entry2->Close();

  cache_->EndEnumeration(&iter1);
  cache_->EndEnumeration(&iter2);
}

TEST_F(DiskCacheBackendTest, FixEnumerators) {
  BackendFixEnumerators();
}

TEST_F(DiskCacheBackendTest, NewEvictionFixEnumerators) {
  SetNewEviction();
  BackendFixEnumerators();
}

void DiskCacheBackendTest::BackendDoomRecent() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry;
  ASSERT_EQ(net::OK, CreateEntry("first", &entry));
  entry->Close();
  ASSERT_EQ(net::OK, CreateEntry("second", &entry));
  entry->Close();

  base::PlatformThread::Sleep(20);
  Time middle = Time::Now();

  ASSERT_EQ(net::OK, CreateEntry("third", &entry));
  entry->Close();
  ASSERT_EQ(net::OK, CreateEntry("fourth", &entry));
  entry->Close();

  base::PlatformThread::Sleep(20);
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_EQ(net::OK, DoomEntriesSince(final));
  ASSERT_EQ(4, cache_->GetEntryCount());

  EXPECT_EQ(net::OK, DoomEntriesSince(middle));
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_EQ(net::OK, OpenEntry("second", &entry));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomRecent) {
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomRecent) {
  SetNewEviction();
  BackendDoomRecent();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomRecent) {
  SetMemoryOnlyMode();
  BackendDoomRecent();
}

void DiskCacheBackendTest::BackendDoomBetween() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry;
  ASSERT_EQ(net::OK, CreateEntry("first", &entry));
  entry->Close();

  base::PlatformThread::Sleep(20);
  Time middle_start = Time::Now();

  ASSERT_EQ(net::OK, CreateEntry("second", &entry));
  entry->Close();
  ASSERT_EQ(net::OK, CreateEntry("third", &entry));
  entry->Close();

  base::PlatformThread::Sleep(20);
  Time middle_end = Time::Now();

  ASSERT_EQ(net::OK, CreateEntry("fourth", &entry));
  entry->Close();
  ASSERT_EQ(net::OK, OpenEntry("fourth", &entry));
  entry->Close();

  base::PlatformThread::Sleep(20);
  Time final = Time::Now();

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_EQ(net::OK, DoomEntriesBetween(middle_start, middle_end));
  ASSERT_EQ(2, cache_->GetEntryCount());

  ASSERT_EQ(net::OK, OpenEntry("fourth", &entry));
  entry->Close();

  EXPECT_EQ(net::OK, DoomEntriesBetween(middle_start, final));
  ASSERT_EQ(1, cache_->GetEntryCount());

  ASSERT_EQ(net::OK, OpenEntry("first", &entry));
  entry->Close();
}

TEST_F(DiskCacheBackendTest, DoomBetween) {
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomBetween) {
  SetNewEviction();
  BackendDoomBetween();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomBetween) {
  SetMemoryOnlyMode();
  BackendDoomBetween();
}

void DiskCacheBackendTest::BackendTransaction(const std::string& name,
                                              int num_entries, bool load) {
  success_ = false;
  ASSERT_TRUE(CopyTestCache(name));
  DisableFirstCleanup();

  if (load) {
    SetMask(0xf);
    SetMaxSize(0x100000);
  } else {
    // Clear the settings from the previous run.
    SetMask(0);
    SetMaxSize(0);
  }

  InitCache();
  ASSERT_EQ(num_entries + 1, cache_->GetEntryCount());

  std::string key("the first key");
  disk_cache::Entry* entry1;
  ASSERT_NE(net::OK, OpenEntry(key, &entry1));

  int actual = cache_->GetEntryCount();
  if (num_entries != actual) {
    ASSERT_TRUE(load);
    // If there is a heavy load, inserting an entry will make another entry
    // dirty (on the hash bucket) so two entries are removed.
    ASSERT_EQ(num_entries - 1, actual);
  }

  delete cache_;
  cache_ = NULL;
  cache_impl_ = NULL;

  ASSERT_TRUE(CheckCacheIntegrity(GetCacheFilePath(), new_eviction_));
  success_ = true;
}

void DiskCacheBackendTest::BackendRecoverInsert() {
  // Tests with an empty cache.
  BackendTransaction("insert_empty1", 0, false);
  ASSERT_TRUE(success_) << "insert_empty1";
  BackendTransaction("insert_empty2", 0, false);
  ASSERT_TRUE(success_) << "insert_empty2";
  BackendTransaction("insert_empty3", 0, false);
  ASSERT_TRUE(success_) << "insert_empty3";

  // Tests with one entry on the cache.
  BackendTransaction("insert_one1", 1, false);
  ASSERT_TRUE(success_) << "insert_one1";
  BackendTransaction("insert_one2", 1, false);
  ASSERT_TRUE(success_) << "insert_one2";
  BackendTransaction("insert_one3", 1, false);
  ASSERT_TRUE(success_) << "insert_one3";

  // Tests with one hundred entries on the cache, tiny index.
  BackendTransaction("insert_load1", 100, true);
  ASSERT_TRUE(success_) << "insert_load1";
  BackendTransaction("insert_load2", 100, true);
  ASSERT_TRUE(success_) << "insert_load2";
}

TEST_F(DiskCacheBackendTest, RecoverInsert) {
  BackendRecoverInsert();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverInsert) {
  SetNewEviction();
  BackendRecoverInsert();
}

void DiskCacheBackendTest::BackendRecoverRemove() {
  // Removing the only element.
  BackendTransaction("remove_one1", 0, false);
  ASSERT_TRUE(success_) << "remove_one1";
  BackendTransaction("remove_one2", 0, false);
  ASSERT_TRUE(success_) << "remove_one2";
  BackendTransaction("remove_one3", 0, false);
  ASSERT_TRUE(success_) << "remove_one3";

  // Removing the head.
  BackendTransaction("remove_head1", 1, false);
  ASSERT_TRUE(success_) << "remove_head1";
  BackendTransaction("remove_head2", 1, false);
  ASSERT_TRUE(success_) << "remove_head2";
  BackendTransaction("remove_head3", 1, false);
  ASSERT_TRUE(success_) << "remove_head3";

  // Removing the tail.
  BackendTransaction("remove_tail1", 1, false);
  ASSERT_TRUE(success_) << "remove_tail1";
  BackendTransaction("remove_tail2", 1, false);
  ASSERT_TRUE(success_) << "remove_tail2";
  BackendTransaction("remove_tail3", 1, false);
  ASSERT_TRUE(success_) << "remove_tail3";

  // Removing with one hundred entries on the cache, tiny index.
  BackendTransaction("remove_load1", 100, true);
  ASSERT_TRUE(success_) << "remove_load1";
  BackendTransaction("remove_load2", 100, true);
  ASSERT_TRUE(success_) << "remove_load2";
  BackendTransaction("remove_load3", 100, true);
  ASSERT_TRUE(success_) << "remove_load3";

#ifdef NDEBUG
  // This case cannot be reverted, so it will assert on debug builds.
  BackendTransaction("remove_one4", 0, false);
  ASSERT_TRUE(success_) << "remove_one4";
  BackendTransaction("remove_head4", 1, false);
  ASSERT_TRUE(success_) << "remove_head4";
#endif
}

TEST_F(DiskCacheBackendTest, RecoverRemove) {
  BackendRecoverRemove();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverRemove) {
  SetNewEviction();
  BackendRecoverRemove();
}

void DiskCacheBackendTest::BackendRecoverWithEviction() {
  success_ = false;
  ASSERT_TRUE(CopyTestCache("insert_load1"));
  DisableFirstCleanup();

  SetMask(0xf);
  SetMaxSize(0x1000);

  // We should not crash here.
  InitCache();
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, RecoverWithEviction) {
  BackendRecoverWithEviction();
}

TEST_F(DiskCacheBackendTest, NewEvictionRecoverWithEviction) {
  SetNewEviction();
  BackendRecoverWithEviction();
}

// Tests dealing with cache files that cannot be recovered.
TEST_F(DiskCacheTest, DeleteOld) {
  ASSERT_TRUE(CopyTestCache("wrong_version"));
  FilePath path = GetCacheFilePath();
  base::Thread cache_thread("CacheThread");
  ASSERT_TRUE(cache_thread.StartWithOptions(
                  base::Thread::Options(MessageLoop::TYPE_IO, 0)));
  TestCompletionCallback cb;

  disk_cache::Backend* cache;
  int rv = disk_cache::BackendImpl::CreateBackend(
               path, true, 0, net::DISK_CACHE, disk_cache::kNoRandom,
               cache_thread.message_loop_proxy(), NULL, &cache, &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  MessageLoopHelper helper;

  ASSERT_TRUE(NULL != cache);
  ASSERT_EQ(0, cache->GetEntryCount());

  delete cache;
}

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidEntry2() {
  ASSERT_TRUE(CopyTestCache("bad_entry"));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry1));
  EXPECT_NE(net::OK, OpenEntry("some other key", &entry2));
  entry1->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidEntry2) {
  BackendInvalidEntry2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry2) {
  SetNewEviction();
  BackendInvalidEntry2();
}

// Tests that we don't crash or hang when enumerating this cache.
void DiskCacheBackendTest::BackendInvalidEntry3() {
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry* entry;
  void* iter = NULL;
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    entry->Close();
  }
}

TEST_F(DiskCacheBackendTest, InvalidEntry3) {
  ASSERT_TRUE(CopyTestCache("dirty_entry3"));
  BackendInvalidEntry3();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry3) {
  ASSERT_TRUE(CopyTestCache("dirty_entry4"));
  SetNewEviction();
  BackendInvalidEntry3();
  DisableIntegrityCheck();
}

// Test that we handle a dirty entry on the LRU list, already replaced with
// the same key, and with hash collisions.
TEST_F(DiskCacheBackendTest, InvalidEntry4) {
  ASSERT_TRUE(CopyTestCache("dirty_entry3"));
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  TrimForTest(false);
}

// Test that we handle a dirty entry on the deleted list, already replaced with
// the same key, and with hash collisions.
TEST_F(DiskCacheBackendTest, InvalidEntry5) {
  ASSERT_TRUE(CopyTestCache("dirty_entry4"));
  SetNewEviction();
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  TrimDeletedListForTest(false);
}

TEST_F(DiskCacheBackendTest, InvalidEntry6) {
  ASSERT_TRUE(CopyTestCache("dirty_entry5"));
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // There is a dirty entry (but marked as clean) at the end, pointing to a
  // deleted entry through the hash collision list. We should not re-insert the
  // deleted entry into the index table.

  TrimForTest(false);
  // The cache should be clean (as detected by CheckCacheIntegrity).
}

// Tests that we don't hang when there is a loop on the hash collision list.
// The test cache could be a result of bug 69135.
TEST_F(DiskCacheBackendTest, BadNextEntry1) {
  ASSERT_TRUE(CopyTestCache("list_loop2"));
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // The second entry points at itselft, and the first entry is not accessible
  // though the index, but it is at the head of the LRU.

  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry("The first key", &entry));
  entry->Close();

  TrimForTest(false);
  TrimForTest(false);
  ASSERT_EQ(net::OK, OpenEntry("The first key", &entry));
  entry->Close();
  EXPECT_EQ(1, cache_->GetEntryCount());
}

// Tests that we don't hang when there is a loop on the hash collision list.
// The test cache could be a result of bug 69135.
TEST_F(DiskCacheBackendTest, BadNextEntry2) {
  ASSERT_TRUE(CopyTestCache("list_loop3"));
  SetMask(0x1);  // 2-entry table.
  SetMaxSize(0x3000);  // 12 kB.
  DisableFirstCleanup();
  InitCache();

  // There is a wide loop of 5 entries.

  disk_cache::Entry* entry;
  ASSERT_NE(net::OK, OpenEntry("Not present key", &entry));
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidEntry6) {
  ASSERT_TRUE(CopyTestCache("bad_rankings3"));
  DisableFirstCleanup();
  SetNewEviction();
  InitCache();

  // The second entry is dirty, but removing it should not corrupt the list.
  disk_cache::Entry* entry;
  ASSERT_NE(net::OK, OpenEntry("the second key", &entry));
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry));

  // This should not delete the cache.
  entry->Doom();
  FlushQueueForTest();
  entry->Close();

  ASSERT_EQ(net::OK, OpenEntry("some other key", &entry));
  entry->Close();
}

// We want to be able to deal with abnormal dirty entries.
void DiskCacheBackendTest::BackendNotMarkedButDirty(const std::string& name) {
  ASSERT_TRUE(CopyTestCache(name));
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, OpenEntry("the first key", &entry1));
  EXPECT_NE(net::OK, OpenEntry("some other key", &entry2));
  entry1->Close();
}

TEST_F(DiskCacheBackendTest, NotMarkedButDirty) {
  BackendNotMarkedButDirty("dirty_entry");
}

TEST_F(DiskCacheBackendTest, NewEvictionNotMarkedButDirty) {
  SetNewEviction();
  BackendNotMarkedButDirty("dirty_entry");
}

TEST_F(DiskCacheBackendTest, NotMarkedButDirty2) {
  BackendNotMarkedButDirty("dirty_entry2");
}

TEST_F(DiskCacheBackendTest, NewEvictionNotMarkedButDirty2) {
  SetNewEviction();
  BackendNotMarkedButDirty("dirty_entry2");
}

// We want to be able to deal with messed up entries on disk.
void DiskCacheBackendTest::BackendInvalidRankings2() {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  FilePath path = GetCacheFilePath();
  DisableFirstCleanup();
  InitCache();

  disk_cache::Entry *entry1, *entry2;
  EXPECT_NE(net::OK, OpenEntry("the first key", &entry1));
  ASSERT_EQ(net::OK, OpenEntry("some other key", &entry2));
  entry2->Close();

  // CheckCacheIntegrity will fail at this point.
  DisableIntegrityCheck();
}

TEST_F(DiskCacheBackendTest, InvalidRankings2) {
  BackendInvalidRankings2();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankings2) {
  SetNewEviction();
  BackendInvalidRankings2();
}

// If the LRU is corrupt, we delete the cache.
void DiskCacheBackendTest::BackendInvalidRankings() {
  disk_cache::Entry* entry;
  void* iter = NULL;
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry));
  entry->Close();
  EXPECT_EQ(2, cache_->GetEntryCount());

  EXPECT_NE(net::OK, OpenNextEntry(&iter, &entry));
  FlushQueueForTest();  // Allow the restart to finish.
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, InvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, InvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

TEST_F(DiskCacheBackendTest, NewEvictionInvalidRankingsFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendInvalidRankings();
}

// If the LRU is corrupt and we have open entries, we disable the cache.
void DiskCacheBackendTest::BackendDisable() {
  disk_cache::Entry *entry1, *entry2;
  void* iter = NULL;
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry1));

  EXPECT_NE(net::OK, OpenNextEntry(&iter, &entry2));
  EXPECT_EQ(0, cache_->GetEntryCount());
  EXPECT_NE(net::OK, CreateEntry("Something new", &entry2));

  entry1->Close();
  FlushQueueForTest();  // Flushing the Close posts a task to restart the cache.
  FlushQueueForTest();  // This one actually allows that task to complete.

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, DisableFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable();
}

// This is another type of corruption on the LRU; disable the cache.
void DiskCacheBackendTest::BackendDisable2() {
  EXPECT_EQ(8, cache_->GetEntryCount());

  disk_cache::Entry* entry;
  void* iter = NULL;
  int count = 0;
  while (OpenNextEntry(&iter, &entry) == net::OK) {
    ASSERT_TRUE(NULL != entry);
    entry->Close();
    count++;
    ASSERT_LT(count, 9);
  };

  FlushQueueForTest();
  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetNewEviction();
  SetDirectMode();
  InitCache();
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, DisableFailure2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableFailure2) {
  ASSERT_TRUE(CopyTestCache("list_loop"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  SetTestMode();  // Fail cache reinitialization.
  BackendDisable2();
}

// If the index size changes when we disable the cache, we should not crash.
void DiskCacheBackendTest::BackendDisable3() {
  disk_cache::Entry *entry1, *entry2;
  void* iter = NULL;
  EXPECT_EQ(2, cache_->GetEntryCount());
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry1));
  entry1->Close();

  EXPECT_NE(net::OK, OpenNextEntry(&iter, &entry2));
  FlushQueueForTest();

  ASSERT_EQ(net::OK, CreateEntry("Something new", &entry2));
  entry2->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess3) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDisable3();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess3) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDisable3();
}

// If we disable the cache, already open entries should work as far as possible.
void DiskCacheBackendTest::BackendDisable4() {
  disk_cache::Entry *entry1, *entry2, *entry3, *entry4;
  void* iter = NULL;
  ASSERT_EQ(net::OK, OpenNextEntry(&iter, &entry1));

  char key2[2000];
  char key3[20000];
  CacheTestFillBuffer(key2, sizeof(key2), true);
  CacheTestFillBuffer(key3, sizeof(key3), true);
  key2[sizeof(key2) - 1] = '\0';
  key3[sizeof(key3) - 1] = '\0';
  ASSERT_EQ(net::OK, CreateEntry(key2, &entry2));
  ASSERT_EQ(net::OK, CreateEntry(key3, &entry3));

  const int kBufSize = 20000;
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(kBufSize));
  memset(buf->data(), 0, kBufSize);
  EXPECT_EQ(100, WriteData(entry2, 0, 0, buf, 100, false));
  EXPECT_EQ(kBufSize, WriteData(entry3, 0, 0, buf, kBufSize, false));

  // This line should disable the cache but not delete it.
  EXPECT_NE(net::OK, OpenNextEntry(&iter, &entry4));
  EXPECT_EQ(0, cache_->GetEntryCount());

  EXPECT_NE(net::OK, CreateEntry("cache is disabled", &entry4));

  EXPECT_EQ(100, ReadData(entry2, 0, 0, buf, 100));
  EXPECT_EQ(100, WriteData(entry2, 0, 0, buf, 100, false));
  EXPECT_EQ(100, WriteData(entry2, 1, 0, buf, 100, false));

  EXPECT_EQ(kBufSize, ReadData(entry3, 0, 0, buf, kBufSize));
  EXPECT_EQ(kBufSize, WriteData(entry3, 0, 0, buf, kBufSize, false));
  EXPECT_EQ(kBufSize, WriteData(entry3, 1, 0, buf, kBufSize, false));

  std::string key = entry2->GetKey();
  EXPECT_EQ(sizeof(key2) - 1, key.size());
  key = entry3->GetKey();
  EXPECT_EQ(sizeof(key3) - 1, key.size());

  entry1->Close();
  entry2->Close();
  entry3->Close();
  FlushQueueForTest();  // Flushing the Close posts a task to restart the cache.
  FlushQueueForTest();  // This one actually allows that task to complete.

  EXPECT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DisableSuccess4) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  InitCache();
  BackendDisable4();
}

TEST_F(DiskCacheBackendTest, NewEvictionDisableSuccess4) {
  ASSERT_TRUE(CopyTestCache("bad_rankings"));
  DisableFirstCleanup();
  SetDirectMode();
  SetNewEviction();
  InitCache();
  BackendDisable4();
}

TEST_F(DiskCacheTest, Backend_UsageStats) {
  MessageLoopHelper helper;

  FilePath path = GetCacheFilePath();
  ASSERT_TRUE(DeleteCache(path));
  scoped_ptr<disk_cache::BackendImpl> cache;
  cache.reset(new disk_cache::BackendImpl(
                  path, base::MessageLoopProxy::CreateForCurrentThread(),
                  NULL));
  ASSERT_TRUE(NULL != cache.get());
  cache->SetUnitTestMode();
  ASSERT_EQ(net::OK, cache->SyncInit());

  // Wait for a callback that never comes... about 2 secs :). The message loop
  // has to run to allow invocation of the usage timer.
  helper.WaitUntilCacheIoFinished(1);
}

void DiskCacheBackendTest::BackendDoomAll() {
  InitCache();
  Time initial = Time::Now();

  disk_cache::Entry *entry1, *entry2;
  ASSERT_EQ(net::OK, CreateEntry("first", &entry1));
  ASSERT_EQ(net::OK, CreateEntry("second", &entry2));
  entry1->Close();
  entry2->Close();

  ASSERT_EQ(net::OK, CreateEntry("third", &entry1));
  ASSERT_EQ(net::OK, CreateEntry("fourth", &entry2));

  ASSERT_EQ(4, cache_->GetEntryCount());
  EXPECT_EQ(net::OK, DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());

  // We should stop posting tasks at some point (if we post any).
  MessageLoop::current()->RunAllPending();

  disk_cache::Entry *entry3, *entry4;
  ASSERT_EQ(net::OK, CreateEntry("third", &entry3));
  ASSERT_EQ(net::OK, CreateEntry("fourth", &entry4));

  EXPECT_EQ(net::OK, DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());

  entry1->Close();
  entry2->Close();
  entry3->Doom();  // The entry should be already doomed, but this must work.
  entry3->Close();
  entry4->Close();

  // Now try with all references released.
  ASSERT_EQ(net::OK, CreateEntry("third", &entry1));
  ASSERT_EQ(net::OK, CreateEntry("fourth", &entry2));
  entry1->Close();
  entry2->Close();

  ASSERT_EQ(2, cache_->GetEntryCount());
  EXPECT_EQ(net::OK, DoomAllEntries());
  ASSERT_EQ(0, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAll) {
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll) {
  SetNewEviction();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, MemoryOnlyDoomAll) {
  SetMemoryOnlyMode();
  BackendDoomAll();
}

TEST_F(DiskCacheBackendTest, AppCacheOnlyDoomAll) {
  SetCacheType(net::APP_CACHE);
  BackendDoomAll();
}

// If the index size changes when we doom the cache, we should not crash.
void DiskCacheBackendTest::BackendDoomAll2() {
  EXPECT_EQ(2, cache_->GetEntryCount());
  EXPECT_EQ(net::OK, DoomAllEntries());

  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry("Something new", &entry));
  entry->Close();

  EXPECT_EQ(1, cache_->GetEntryCount());
}

TEST_F(DiskCacheBackendTest, DoomAll2) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  InitCache();
  BackendDoomAll2();
}

TEST_F(DiskCacheBackendTest, NewEvictionDoomAll2) {
  ASSERT_TRUE(CopyTestCache("bad_rankings2"));
  DisableFirstCleanup();
  SetMaxSize(20 * 1024 * 1024);
  SetNewEviction();
  InitCache();
  BackendDoomAll2();
}

// We should be able to create the same entry on multiple simultaneous instances
// of the cache.
TEST_F(DiskCacheTest, MultipleInstances) {
  ScopedTestCache store1;
  ScopedTestCache store2("cache_test2");
  ScopedTestCache store3("cache_test3");
  base::Thread cache_thread("CacheThread");
  ASSERT_TRUE(cache_thread.StartWithOptions(
                  base::Thread::Options(MessageLoop::TYPE_IO, 0)));
  TestCompletionCallback cb;

  const int kNumberOfCaches = 2;
  disk_cache::Backend* cache[kNumberOfCaches];

  int rv = disk_cache::BackendImpl::CreateBackend(
               store1.path(), false, 0, net::DISK_CACHE, disk_cache::kNone,
               cache_thread.message_loop_proxy(), NULL, &cache[0], &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));
  rv = disk_cache::BackendImpl::CreateBackend(
           store2.path(), false, 0, net::MEDIA_CACHE, disk_cache::kNone,
           cache_thread.message_loop_proxy(), NULL, &cache[1], &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  ASSERT_TRUE(cache[0] != NULL && cache[1] != NULL);

  std::string key("the first key");
  disk_cache::Entry* entry;
  for (int i = 0; i < kNumberOfCaches; i++) {
    rv = cache[i]->CreateEntry(key, &entry, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));
    entry->Close();
  }
  delete cache[0];
  delete cache[1];
}

// Test the six regions of the curve that determines the max cache size.
TEST_F(DiskCacheTest, AutomaticMaxSize) {
  const int kDefaultSize = 80 * 1024 * 1024;
  int64 large_size = kDefaultSize;
  int64 largest_size = kint32max;

  // Region 1: expected = available * 0.8
  EXPECT_EQ((kDefaultSize - 1) * 8 / 10,
            disk_cache::PreferedCacheSize(large_size - 1));
  EXPECT_EQ(kDefaultSize * 8 / 10,
            disk_cache::PreferedCacheSize(large_size));
  EXPECT_EQ(kDefaultSize - 1,
            disk_cache::PreferedCacheSize(large_size * 10 / 8 - 1));

  // Region 2: expected = default_size
  EXPECT_EQ(kDefaultSize,
            disk_cache::PreferedCacheSize(large_size * 10 / 8));
  EXPECT_EQ(kDefaultSize,
            disk_cache::PreferedCacheSize(large_size * 10 - 1));

  // Region 3: expected = available * 0.1
  EXPECT_EQ(kDefaultSize,
            disk_cache::PreferedCacheSize(large_size * 10));
  EXPECT_EQ((kDefaultSize * 25 - 1) / 10,
            disk_cache::PreferedCacheSize(large_size * 25 - 1));

  // Region 4: expected = default_size * 2.5
  EXPECT_EQ(kDefaultSize * 25 / 10,
            disk_cache::PreferedCacheSize(large_size * 25));
  EXPECT_EQ(kDefaultSize * 25 / 10,
            disk_cache::PreferedCacheSize(large_size * 100 - 1));
  EXPECT_EQ(kDefaultSize * 25 / 10,
            disk_cache::PreferedCacheSize(large_size * 100));
  EXPECT_EQ(kDefaultSize * 25 / 10,
            disk_cache::PreferedCacheSize(large_size * 250 - 1));

  // Region 5: expected = available * 0.1
  EXPECT_EQ(kDefaultSize * 25 / 10,
            disk_cache::PreferedCacheSize(large_size * 250));
  EXPECT_EQ(kint32max - 1,
            disk_cache::PreferedCacheSize(largest_size * 100 - 1));

  // Region 6: expected = kint32max
  EXPECT_EQ(kint32max,
            disk_cache::PreferedCacheSize(largest_size * 100));
  EXPECT_EQ(kint32max,
            disk_cache::PreferedCacheSize(largest_size * 10000));
}

// Tests that we can "migrate" a running instance from one experiment group to
// another.
TEST_F(DiskCacheBackendTest, Histograms) {
  SetDirectMode();
  InitCache();
  disk_cache::BackendImpl* backend_ = cache_impl_;  // Needed be the macro.

  for (int i = 1; i < 3; i++) {
    CACHE_UMA(HOURS, "FillupTime", i, 28);
  }
}

// Make sure that we keep the total memory used by the internal buffers under
// control.
TEST_F(DiskCacheBackendTest, TotalBuffersSize1) {
  SetDirectMode();
  InitCache();
  std::string key("the first key");
  disk_cache::Entry* entry;
  ASSERT_EQ(net::OK, CreateEntry(key, &entry));

  const int kSize = 200;
  scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(kSize));
  CacheTestFillBuffer(buffer->data(), kSize, true);

  for (int i = 0; i < 10; i++) {
    SCOPED_TRACE(i);
    // Allocate 2MB for this entry.
    EXPECT_EQ(kSize, WriteData(entry, 0, 0, buffer, kSize, true));
    EXPECT_EQ(kSize, WriteData(entry, 1, 0, buffer, kSize, true));
    EXPECT_EQ(kSize, WriteData(entry, 0, 1024 * 1024, buffer, kSize, false));
    EXPECT_EQ(kSize, WriteData(entry, 1, 1024 * 1024, buffer, kSize, false));

    // Delete one of the buffers and truncate the other.
    EXPECT_EQ(0, WriteData(entry, 0, 0, buffer, 0, true));
    EXPECT_EQ(0, WriteData(entry, 1, 10, buffer, 0, true));

    // Delete the second buffer, writing 10 bytes to disk.
    entry->Close();
    ASSERT_EQ(net::OK, OpenEntry(key, &entry));
  }

  entry->Close();
  EXPECT_EQ(0, cache_impl_->GetTotalBuffersSize());
}

// This test assumes at least 150MB of system memory.
TEST_F(DiskCacheBackendTest, TotalBuffersSize2) {
  SetDirectMode();
  InitCache();

  const int kOneMB = 1024 * 1024;
  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB, cache_impl_->GetTotalBuffersSize());

  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB * 2, cache_impl_->GetTotalBuffersSize());

  EXPECT_TRUE(cache_impl_->IsAllocAllowed(0, kOneMB));
  EXPECT_EQ(kOneMB * 3, cache_impl_->GetTotalBuffersSize());

  cache_impl_->BufferDeleted(kOneMB);
  EXPECT_EQ(kOneMB * 2, cache_impl_->GetTotalBuffersSize());

  // Check the upper limit.
  EXPECT_FALSE(cache_impl_->IsAllocAllowed(0, 30 * kOneMB));

  for (int i = 0; i < 30; i++)
    cache_impl_->IsAllocAllowed(0, kOneMB);  // Ignore the result.

  EXPECT_FALSE(cache_impl_->IsAllocAllowed(0, kOneMB));
}

// Tests that sharing of external files works and we are able to delete the
// files when we need to.
TEST_F(DiskCacheBackendTest, FileSharing) {
  SetDirectMode();
  InitCache();

  disk_cache::Addr address(0x80000001);
  ASSERT_TRUE(cache_impl_->CreateExternalFile(&address));
  FilePath name = cache_impl_->GetFileName(address);

  scoped_refptr<disk_cache::File> file(new disk_cache::File(false));
  file->Init(name);

#if defined(OS_WIN)
  DWORD sharing = FILE_SHARE_READ | FILE_SHARE_WRITE;
  DWORD access = GENERIC_READ | GENERIC_WRITE;
  base::win::ScopedHandle file2(CreateFile(
      name.value().c_str(), access, sharing, NULL, OPEN_EXISTING, 0, NULL));
  EXPECT_FALSE(file2.IsValid());

  sharing |= FILE_SHARE_DELETE;
  file2.Set(CreateFile(name.value().c_str(), access, sharing, NULL,
                       OPEN_EXISTING, 0, NULL));
  EXPECT_TRUE(file2.IsValid());
#endif

  EXPECT_TRUE(file_util::Delete(name, false));

  // We should be able to use the file.
  const int kSize = 200;
  char buffer1[kSize];
  char buffer2[kSize];
  memset(buffer1, 't', kSize);
  memset(buffer2, 0, kSize);
  EXPECT_TRUE(file->Write(buffer1, kSize, 0));
  EXPECT_TRUE(file->Read(buffer2, kSize, 0));
  EXPECT_EQ(0, memcmp(buffer1, buffer2, kSize));

  EXPECT_TRUE(disk_cache::DeleteCacheFile(name));
}
