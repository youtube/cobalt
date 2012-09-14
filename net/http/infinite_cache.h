// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the interface of a simulated infinite cache. The purpose of this
// code is to evaluate the performance of an HTTP cache that is not constrained
// to evict resources.

#ifndef NET_HTTP_INFINITE_CACHE_H_
#define NET_HTTP_INFINITE_CACHE_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/base/net_export.h"

class FilePath;

namespace base {
class SequencedTaskRunner;
class SequencedWorkerPool;
}

namespace net {

class HttpCache;
class HttpResponseInfo;
class InfiniteCache;
struct HttpRequestInfo;

// An InfiniteCacheTransaction is paired with an HttpCache::Transaction to track
// every request that goes through the HttpCache. This object is notified when
// relevant steps are reached while processing the request.
class NET_EXPORT_PRIVATE InfiniteCacheTransaction {
 public:
  explicit InfiniteCacheTransaction(InfiniteCache* cache);
  ~InfiniteCacheTransaction();

  // Called when a new HttpTransaction is started.
  void OnRequestStart(const HttpRequestInfo* request);

  // Called when the response headers are available.
  void OnResponseReceived(const HttpResponseInfo* response);

  // Called when the response data is received from the network.
  void OnDataRead(const char* data, int data_len);

  // Called when the resource is marked as truncated by the HttpCache.
  void OnTruncatedResponse();

  // Called when the resource is served from the cache, so OnDataRead will not
  // be called for this request.
  void OnServedFromCache();

 private:
  friend class InfiniteCache;
  struct ResourceData;
  void Finish();

  base::WeakPtr<InfiniteCache> cache_;
  scoped_ptr<ResourceData> resource_data_;
  bool must_doom_entry_;
  DISALLOW_COPY_AND_ASSIGN(InfiniteCacheTransaction);
};

// An InfiniteCache is paired with an HttpCache instance to simulate an infinite
// backend storage.
class NET_EXPORT_PRIVATE InfiniteCache
    : public base::SupportsWeakPtr<InfiniteCache> {
 public:
  InfiniteCache();
  ~InfiniteCache();

  // Initializes this object to start tracking requests. |path| is the location
  // of the file to use to store data; it can be empty, in which case the data
  // will not be persisted to disk.
  void Init(const FilePath& path);

  InfiniteCacheTransaction* CreateInfiniteCacheTransaction();

  // Removes all data for this experiment. Returns a net error code.
  int DeleteData(const CompletionCallback& callback);

  // Removes requests between |initial_time| and |end_time|.
  int DeleteDataBetween(base::Time initial_time,
                        base::Time end_time,
                        const CompletionCallback& callback);

  // Returns the number of elements currently tracked.
  int QueryItemsForTest(const CompletionCallback& callback);

  // Flush the data to disk, preventing flushing when the destructor runs. Any
  // data added to the cache after calling this method will not be written to
  // disk, unless this method is called again to do so.
  int FlushDataForTest(const CompletionCallback& callback);

 private:
  class Worker;
  friend class base::RepeatingTimer<InfiniteCache>;
  friend class InfiniteCacheTransaction;

  std::string GenerateKey(const HttpRequestInfo* request);
  void ProcessResource(scoped_ptr<InfiniteCacheTransaction::ResourceData> data);
  void OnTimer();

  scoped_refptr<Worker> worker_;
  scoped_refptr<base::SequencedWorkerPool> worker_pool_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::RepeatingTimer<InfiniteCache> timer_;

  DISALLOW_COPY_AND_ASSIGN(InfiniteCache);
};

}  // namespace net

#endif  // NET_HTTP_INFINITE_CACHE_H_
