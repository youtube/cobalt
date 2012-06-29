// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_base.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/disk_cache_test_util.h"
#include "net/disk_cache/mem_backend_impl.h"

DiskCacheTest::DiskCacheTest() {
  cache_path_ = GetCacheFilePath();
  if (!MessageLoop::current())
    message_loop_.reset(new MessageLoopForIO());
}

DiskCacheTest::~DiskCacheTest() {
}

bool DiskCacheTest::CopyTestCache(const std::string& name) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII(name);

  if (!CleanupCacheDir())
    return false;
  return file_util::CopyDirectory(path, cache_path_, false);
}

bool DiskCacheTest::CleanupCacheDir() {
  return DeleteCache(cache_path_);
}

void DiskCacheTest::TearDown() {
  MessageLoop::current()->RunAllPending();
}

DiskCacheTestWithCache::DiskCacheTestWithCache()
    : cache_(NULL),
      cache_impl_(NULL),
      mem_cache_(NULL),
      mask_(0),
      size_(0),
      type_(net::DISK_CACHE),
      memory_only_(false),
      implementation_(false),
      force_creation_(false),
      new_eviction_(false),
      first_cleanup_(true),
      integrity_(true),
      use_current_thread_(false),
      cache_thread_("CacheThread") {
}

DiskCacheTestWithCache::~DiskCacheTestWithCache() {}

void DiskCacheTestWithCache::InitCache() {
  if (mask_ || new_eviction_)
    implementation_ = true;

  if (memory_only_)
    InitMemoryCache();
  else
    InitDiskCache();

  ASSERT_TRUE(NULL != cache_);
  if (first_cleanup_)
    ASSERT_EQ(0, cache_->GetEntryCount());
}

// We are expected to leak memory when simulating crashes.
void DiskCacheTestWithCache::SimulateCrash() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  net::TestCompletionCallback cb;
  int rv = cache_impl_->FlushQueueForTest(cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));
  cache_impl_->ClearRefCountForTest();

  delete cache_impl_;
  EXPECT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, mask_));

  InitDiskCacheImpl();
}

void DiskCacheTestWithCache::SetTestMode() {
  ASSERT_TRUE(implementation_ && !memory_only_);
  cache_impl_->SetUnitTestMode();
}

void DiskCacheTestWithCache::SetMaxSize(int size) {
  size_ = size;
  if (cache_impl_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size));

  if (mem_cache_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size));
}

int DiskCacheTestWithCache::OpenEntry(const std::string& key,
                                      disk_cache::Entry** entry) {
  net::TestCompletionCallback cb;
  int rv = cache_->OpenEntry(key, entry, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::CreateEntry(const std::string& key,
                                        disk_cache::Entry** entry) {
  net::TestCompletionCallback cb;
  int rv = cache_->CreateEntry(key, entry, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomEntry(const std::string& key) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntry(key, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomAllEntries() {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomAllEntries(cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomEntriesBetween(const base::Time initial_time,
                                               const base::Time end_time) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntriesBetween(initial_time, end_time, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::DoomEntriesSince(const base::Time initial_time) {
  net::TestCompletionCallback cb;
  int rv = cache_->DoomEntriesSince(initial_time, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::OpenNextEntry(void** iter,
                                          disk_cache::Entry** next_entry) {
  net::TestCompletionCallback cb;
  int rv = cache_->OpenNextEntry(iter, next_entry, cb.callback());
  return cb.GetResult(rv);
}

void DiskCacheTestWithCache::FlushQueueForTest() {
  if (memory_only_ || !cache_impl_)
    return;

  net::TestCompletionCallback cb;
  int rv = cache_impl_->FlushQueueForTest(cb.callback());
  EXPECT_EQ(net::OK, cb.GetResult(rv));
}

void DiskCacheTestWithCache::RunTaskForTest(const base::Closure& closure) {
  if (memory_only_ || !cache_impl_) {
    closure.Run();
    return;
  }

  net::TestCompletionCallback cb;
  int rv = cache_impl_->RunTaskForTest(closure, cb.callback());
  EXPECT_EQ(net::OK, cb.GetResult(rv));
}

int DiskCacheTestWithCache::ReadData(disk_cache::Entry* entry, int index,
                                     int offset, net::IOBuffer* buf, int len) {
  net::TestCompletionCallback cb;
  int rv = entry->ReadData(index, offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::WriteData(disk_cache::Entry* entry, int index,
                                      int offset, net::IOBuffer* buf, int len,
                                      bool truncate) {
  net::TestCompletionCallback cb;
  int rv = entry->WriteData(index, offset, buf, len, cb.callback(), truncate);
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::ReadSparseData(disk_cache::Entry* entry,
                                           int64 offset, net::IOBuffer* buf,
                                           int len) {
  net::TestCompletionCallback cb;
  int rv = entry->ReadSparseData(offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

int DiskCacheTestWithCache::WriteSparseData(disk_cache::Entry* entry,
                                            int64 offset,
                                            net::IOBuffer* buf, int len) {
  net::TestCompletionCallback cb;
  int rv = entry->WriteSparseData(offset, buf, len, cb.callback());
  return cb.GetResult(rv);
}

void DiskCacheTestWithCache::TrimForTest(bool empty) {
  RunTaskForTest(base::Bind(&disk_cache::BackendImpl::TrimForTest,
                            base::Unretained(cache_impl_),
                            empty));
}

void DiskCacheTestWithCache::TrimDeletedListForTest(bool empty) {
  RunTaskForTest(base::Bind(&disk_cache::BackendImpl::TrimDeletedListForTest,
                            base::Unretained(cache_impl_),
                            empty));
}

void DiskCacheTestWithCache::AddDelay() {
  base::Time initial = base::Time::Now();
  while (base::Time::Now() <= initial) {
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(1));
  };
}

void DiskCacheTestWithCache::TearDown() {
  MessageLoop::current()->RunAllPending();
  delete cache_;
  if (cache_thread_.IsRunning())
    cache_thread_.Stop();

  if (!memory_only_ && integrity_) {
    EXPECT_TRUE(CheckCacheIntegrity(cache_path_, new_eviction_, mask_));
  }

  PlatformTest::TearDown();
}

void DiskCacheTestWithCache::InitMemoryCache() {
  if (!implementation_) {
    cache_ = disk_cache::MemBackendImpl::CreateBackend(size_, NULL);
    return;
  }

  mem_cache_ = new disk_cache::MemBackendImpl(NULL);
  cache_ = mem_cache_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    EXPECT_TRUE(mem_cache_->SetMaxSize(size_));

  ASSERT_TRUE(mem_cache_->Init());
}

void DiskCacheTestWithCache::InitDiskCache() {
  if (first_cleanup_)
    ASSERT_TRUE(CleanupCacheDir());

  if (!cache_thread_.IsRunning()) {
    EXPECT_TRUE(cache_thread_.StartWithOptions(
                    base::Thread::Options(MessageLoop::TYPE_IO, 0)));
  }
  ASSERT_TRUE(cache_thread_.message_loop() != NULL);

  if (implementation_)
    return InitDiskCacheImpl();

  scoped_refptr<base::MessageLoopProxy> thread =
      use_current_thread_ ? base::MessageLoopProxy::current() :
                            cache_thread_.message_loop_proxy();

  net::TestCompletionCallback cb;
  int rv = disk_cache::BackendImpl::CreateBackend(
               cache_path_, force_creation_, size_, type_,
               disk_cache::kNoRandom, thread, NULL, &cache_, cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));
}

void DiskCacheTestWithCache::InitDiskCacheImpl() {
  scoped_refptr<base::MessageLoopProxy> thread =
      use_current_thread_ ? base::MessageLoopProxy::current() :
                            cache_thread_.message_loop_proxy();
  if (mask_)
    cache_impl_ = new disk_cache::BackendImpl(cache_path_, mask_, thread, NULL);
  else
    cache_impl_ = new disk_cache::BackendImpl(cache_path_, thread, NULL);

  cache_ = cache_impl_;
  ASSERT_TRUE(NULL != cache_);

  if (size_)
    EXPECT_TRUE(cache_impl_->SetMaxSize(size_));

  if (new_eviction_)
    cache_impl_->SetNewEviction();

  cache_impl_->SetType(type_);
  cache_impl_->SetFlags(disk_cache::kNoRandom);
  net::TestCompletionCallback cb;
  int rv = cache_impl_->Init(cb.callback());
  ASSERT_EQ(net::OK, cb.GetResult(rv));
}
