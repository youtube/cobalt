// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
#define NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/timer.h"
#include "build/build_config.h"

// Re-creates a given test file inside the cache test folder.
bool CreateCacheTestFile(const FilePath& name);

// Deletes all file son the cache.
bool DeleteCache(const FilePath& path);

// Copies a set of cache files from the data folder to the test folder.
bool CopyTestCache(const std::string& name);

// Gets the path to the cache test folder.
FilePath GetCacheFilePath();

// Fills buffer with random values (may contain nulls unless no_nulls is true).
void CacheTestFillBuffer(char* buffer, size_t len, bool no_nulls);

// Generates a random key of up to 200 bytes.
std::string GenerateKey(bool same_length);

// Returns true if the cache is not corrupt.
bool CheckCacheIntegrity(const FilePath& path, bool new_eviction);

// Helper class which ensures that the cache dir returned by GetCacheFilePath
// exists and is clear in ctor and that the directory gets deleted in dtor.
class ScopedTestCache {
 public:
  ScopedTestCache();
  // Use a specific folder name.
  explicit ScopedTestCache(const std::string& name);
  ~ScopedTestCache();

  FilePath path() const { return path_; }

 private:
  const FilePath path_;  // Path to the cache test folder.

  DISALLOW_COPY_AND_ASSIGN(ScopedTestCache);
};

// -----------------------------------------------------------------------

// Simple callback to process IO completions from the cache. It allows tests
// with multiple simultaneous IO operations.
class CallbackTest : public CallbackRunner< Tuple1<int> >  {
 public:
  explicit CallbackTest(bool reuse);
  virtual ~CallbackTest();

  int result() const { return result_; }
  virtual void RunWithParams(const Tuple1<int>& params);

 private:
  int result_;
  int reuse_;
  DISALLOW_COPY_AND_ASSIGN(CallbackTest);
};

// -----------------------------------------------------------------------

// Simple helper to deal with the message loop on a test.
class MessageLoopHelper {
 public:
  MessageLoopHelper();
  ~MessageLoopHelper();

  // Run the message loop and wait for num_callbacks before returning. Returns
  // false if we are waiting to long.
  bool WaitUntilCacheIoFinished(int num_callbacks);

 private:
  // Sets the number of callbacks that can be received so far.
  void ExpectCallbacks(int num_callbacks) {
    num_callbacks_ = num_callbacks;
    num_iterations_ = last_ = 0;
    completed_ = false;
  }

  // Called periodically to test if WaitUntilCacheIoFinished should return.
  void TimerExpired();

  base::RepeatingTimer<MessageLoopHelper> timer_;
  int num_callbacks_;
  int num_iterations_;
  int last_;
  bool completed_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopHelper);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_TEST_UTIL_H_
