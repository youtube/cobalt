// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/disk_cache_test_util.h"

#include "base/logging.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/path_service.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/file.h"

using base::Time;
using base::TimeDelta;

namespace {

FilePath BuildCachePath(const std::string& name) {
  FilePath path;
  PathService::Get(base::DIR_TEMP, &path);  // Ignore return value;
  path = path.AppendASCII(name);
  if (!file_util::PathExists(path))
    file_util::CreateDirectory(path);

  return path;
}

}  // namespace.

std::string GenerateKey(bool same_length) {
  char key[200];
  CacheTestFillBuffer(key, sizeof(key), same_length);

  key[199] = '\0';
  return std::string(key);
}

void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i] && no_nulls)
      buffer[i] = 'g';
  }
  if (len && !buffer[0])
    buffer[0] = 'g';
}

FilePath GetCacheFilePath() {
  return BuildCachePath("cache_test");
}

bool CreateCacheTestFile(const FilePath& name) {
  int flags = base::PLATFORM_FILE_CREATE_ALWAYS |
              base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE;

  scoped_refptr<disk_cache::File> file(new disk_cache::File(
      base::CreatePlatformFile(name, flags, NULL, NULL)));
  if (!file->IsValid())
    return false;

  file->SetLength(4 * 1024 * 1024);
  return true;
}

bool DeleteCache(const FilePath& path) {
  disk_cache::DeleteCache(path, false);
  return true;
}

bool CopyTestCache(const std::string& name) {
  FilePath path;
  PathService::Get(base::DIR_SOURCE_ROOT, &path);
  path = path.AppendASCII("net");
  path = path.AppendASCII("data");
  path = path.AppendASCII("cache_tests");
  path = path.AppendASCII(name);

  FilePath dest = GetCacheFilePath();
  if (!DeleteCache(dest))
    return false;
  return file_util::CopyDirectory(path, dest, false);
}

bool CheckCacheIntegrity(const FilePath& path, bool new_eviction) {
  scoped_ptr<disk_cache::BackendImpl> cache(new disk_cache::BackendImpl(
      path, base::MessageLoopProxy::CreateForCurrentThread(), NULL));
  if (!cache.get())
    return false;
  if (new_eviction)
    cache->SetNewEviction();
  cache->SetFlags(disk_cache::kNoRandom);
  if (cache->SyncInit() != net::OK)
    return false;
  return cache->SelfCheck() >= 0;
}

ScopedTestCache::ScopedTestCache() : path_(GetCacheFilePath()) {
  bool result = DeleteCache(path_);
  DCHECK(result);
}

ScopedTestCache::ScopedTestCache(const std::string& name)
    : path_(BuildCachePath(name)) {
  bool result = DeleteCache(path_);
  DCHECK(result);
}

ScopedTestCache::~ScopedTestCache() {
  file_util::Delete(path(), true);
}

// -----------------------------------------------------------------------

volatile int g_cache_tests_received = 0;
volatile bool g_cache_tests_error = 0;

CallbackTest::CallbackTest(bool reuse) : result_(-1), reuse_(reuse ? 0 : 1) {}

CallbackTest::~CallbackTest() {}

// On the actual callback, increase the number of tests received and check for
// errors (an unexpected test received)
void CallbackTest::RunWithParams(const Tuple1<int>& params) {
  if (reuse_) {
    DCHECK(1 == reuse_);
    if (2 == reuse_)
      g_cache_tests_error = true;
    reuse_++;
  }

  result_ = params.a;
  g_cache_tests_received++;
}

// -----------------------------------------------------------------------

MessageLoopHelper::MessageLoopHelper()
    : num_callbacks_(0),
      num_iterations_(0),
      last_(0),
      completed_(false) {
  // Create a recurrent timer of 50 mS.
  timer_.Start(
      TimeDelta::FromMilliseconds(50), this, &MessageLoopHelper::TimerExpired);
}

MessageLoopHelper::~MessageLoopHelper() {
}

bool MessageLoopHelper::WaitUntilCacheIoFinished(int num_callbacks) {
  if (num_callbacks == g_cache_tests_received)
    return true;

  ExpectCallbacks(num_callbacks);
  MessageLoop::current()->Run();
  return completed_;
}

// Quits the message loop when all callbacks are called or we've been waiting
// too long for them (2 secs without a callback).
void MessageLoopHelper::TimerExpired() {
  if (g_cache_tests_received > num_callbacks_) {
    NOTREACHED();
  } else if (g_cache_tests_received == num_callbacks_) {
    completed_ = true;
    MessageLoop::current()->Quit();
  } else {
    // Not finished yet. See if we have to abort.
    if (last_ == g_cache_tests_received)
      num_iterations_++;
    else
      last_ = g_cache_tests_received;
    if (40 == num_iterations_)
      MessageLoop::current()->Quit();
  }
}
