// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a mock of the http cache and related testing classes. To be fair, it
// is not really a mock http cache given that it uses the real implementation of
// the http cache, but it has fake implementations of all required components,
// so it is useful for unit tests at the http layer.

#ifndef NET_HTTP_MOCK_HTTP_CACHE_H_
#define NET_HTTP_MOCK_HTTP_CACHE_H_
#pragma once

#include "base/hash_tables.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_unittest.h"

//-----------------------------------------------------------------------------
// Mock disk cache (a very basic memory cache implementation).

class MockDiskEntry : public disk_cache::Entry,
                      public base::RefCounted<MockDiskEntry> {
 public:
  MockDiskEntry();
  explicit MockDiskEntry(const std::string& key);

  bool is_doomed() const { return doomed_; }

  virtual void Doom() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual std::string GetKey() const OVERRIDE;
  virtual base::Time GetLastUsed() const OVERRIDE;
  virtual base::Time GetLastModified() const OVERRIDE;
  virtual int32 GetDataSize(int index) const OVERRIDE;
  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::OldCompletionCallback* callback) OVERRIDE;
  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::OldCompletionCallback* callback,
                        bool truncate) OVERRIDE;
  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             net::OldCompletionCallback* callback) OVERRIDE;
  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::OldCompletionCallback* callback) OVERRIDE;
  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                net::OldCompletionCallback* callback) OVERRIDE;
  virtual bool CouldBeSparse() const OVERRIDE;
  virtual void CancelSparseIO() OVERRIDE;
  virtual int ReadyForSparseIO(
      net::OldCompletionCallback* completion_callback) OVERRIDE;

  // Fail most subsequent requests.
  void set_fail_requests() { fail_requests_ = true; }

  // If |value| is true, don't deliver any completion callbacks until called
  // again with |value| set to false.  Caution: remember to enable callbacks
  // again or all subsequent tests will fail.
  static void IgnoreCallbacks(bool value);

 private:
  friend class base::RefCounted<MockDiskEntry>;
  struct CallbackInfo;

  virtual ~MockDiskEntry();

  // Unlike the callbacks for MockHttpTransaction, we want this one to run even
  // if the consumer called Close on the MockDiskEntry.  We achieve that by
  // leveraging the fact that this class is reference counted.
  void CallbackLater(net::OldCompletionCallback* callback, int result);

  void RunCallback(net::OldCompletionCallback* callback, int result);

  // When |store| is true, stores the callback to be delivered later; otherwise
  // delivers any callback previously stored.
  static void StoreAndDeliverCallbacks(bool store, MockDiskEntry* entry,
                                       net::OldCompletionCallback* callback,
                                       int result);

  static const int kNumCacheEntryDataIndices = 3;

  std::string key_;
  std::vector<char> data_[kNumCacheEntryDataIndices];
  int test_mode_;
  bool doomed_;
  bool sparse_;
  bool fail_requests_;
  bool busy_;
  bool delayed_;
  static bool cancel_;
  static bool ignore_callbacks_;
};

class MockDiskCache : public disk_cache::Backend {
 public:
  MockDiskCache();
  virtual ~MockDiskCache();

  virtual int32 GetEntryCount() const OVERRIDE;
  virtual int OpenEntry(const std::string& key, disk_cache::Entry** entry,
                        net::OldCompletionCallback* callback) OVERRIDE;
  virtual int CreateEntry(const std::string& key, disk_cache::Entry** entry,
                          net::OldCompletionCallback* callback) OVERRIDE;
  virtual int DoomEntry(const std::string& key,
                        net::OldCompletionCallback* callback) OVERRIDE;
  virtual int DoomAllEntries(net::OldCompletionCallback* callback) OVERRIDE;
  virtual int DoomEntriesBetween(const base::Time initial_time,
                                 const base::Time end_time,
                                 net::OldCompletionCallback* callback) OVERRIDE;
  virtual int DoomEntriesSince(const base::Time initial_time,
                               net::OldCompletionCallback* callback) OVERRIDE;
  virtual int OpenNextEntry(void** iter, disk_cache::Entry** next_entry,
                            net::OldCompletionCallback* callback) OVERRIDE;
  virtual void EndEnumeration(void** iter) OVERRIDE;
  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) OVERRIDE;
  virtual void OnExternalCacheHit(const std::string& key) OVERRIDE;

  // Returns number of times a cache entry was successfully opened.
  int open_count() const { return open_count_; }

  // Returns number of times a cache entry was successfully created.
  int create_count() const { return create_count_; }

  // Fail any subsequent CreateEntry and OpenEntry.
  void set_fail_requests() { fail_requests_ = true; }

  // Return entries that fail some of their requests.
  void set_soft_failures(bool value) { soft_failures_ = value; }

  // Makes sure that CreateEntry is not called twice for a given key.
  void set_double_create_check(bool value) { double_create_check_ = value; }

  void ReleaseAll();

 private:
  typedef base::hash_map<std::string, MockDiskEntry*> EntryMap;
  class CallbackRunner;

  void CallbackLater(net::OldCompletionCallback* callback, int result);

  EntryMap entries_;
  int open_count_;
  int create_count_;
  bool fail_requests_;
  bool soft_failures_;
  bool double_create_check_;
};

class MockBackendFactory : public net::HttpCache::BackendFactory {
 public:
  virtual int CreateBackend(net::NetLog* net_log,
                            disk_cache::Backend** backend,
                            net::OldCompletionCallback* callback) OVERRIDE;
};

class MockHttpCache {
 public:
  MockHttpCache();
  explicit MockHttpCache(net::HttpCache::BackendFactory* disk_cache_factory);

  net::HttpCache* http_cache() { return &http_cache_; }

  MockNetworkLayer* network_layer() {
    return static_cast<MockNetworkLayer*>(http_cache_.network_layer());
  }
  MockDiskCache* disk_cache();

  // Helper function for reading response info from the disk cache.
  static bool ReadResponseInfo(disk_cache::Entry* disk_entry,
                               net::HttpResponseInfo* response_info,
                               bool* response_truncated);

  // Helper function for writing response info into the disk cache.
  static bool WriteResponseInfo(disk_cache::Entry* disk_entry,
                                const net::HttpResponseInfo* response_info,
                                bool skip_transient_headers,
                                bool response_truncated);

  // Helper function to synchronously open a backend entry.
  bool OpenBackendEntry(const std::string& key, disk_cache::Entry** entry);

  // Helper function to synchronously create a backend entry.
  bool CreateBackendEntry(const std::string& key, disk_cache::Entry** entry,
                          net::NetLog* net_log);

  // Returns the test mode after considering the global override.
  static int GetTestMode(int test_mode);

  // Overrides the test mode for a given operation. Remember to reset it after
  // the test! (by setting test_mode to zero).
  static void SetTestMode(int test_mode);

 private:
  net::HttpCache http_cache_;
};

// This version of the disk cache doesn't invoke CreateEntry callbacks.
class MockDiskCacheNoCB : public MockDiskCache {
  virtual int CreateEntry(const std::string& key, disk_cache::Entry** entry,
                          net::OldCompletionCallback* callback) OVERRIDE;
};

class MockBackendNoCbFactory : public net::HttpCache::BackendFactory {
 public:
  virtual int CreateBackend(net::NetLog* net_log,
                            disk_cache::Backend** backend,
                            net::OldCompletionCallback* callback) OVERRIDE;
};

// This backend factory allows us to control the backend instantiation.
class MockBlockingBackendFactory : public net::HttpCache::BackendFactory {
 public:
  MockBlockingBackendFactory();

  virtual int CreateBackend(net::NetLog* net_log,
                            disk_cache::Backend** backend,
                            net::OldCompletionCallback* callback) OVERRIDE;

  // Completes the backend creation. Any blocked call will be notified via the
  // provided callback.
  void FinishCreation();

  disk_cache::Backend** backend() { return backend_; }
  void set_fail(bool fail) { fail_ = fail; }

  net::OldCompletionCallback* callback() { return callback_; }

 private:
  int Result() { return fail_ ? net::ERR_FAILED : net::OK; }

  disk_cache::Backend** backend_;
  net::OldCompletionCallback* callback_;
  bool block_;
  bool fail_;
};

#endif  // NET_HTTP_MOCK_HTTP_CACHE_H_
