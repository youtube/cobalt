// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/infinite_cache.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/threading/platform_thread.h"
#include "base/time.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_transaction_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using net::InfiniteCache;
using net::InfiniteCacheTransaction;

namespace {

void StartRequest(const MockTransaction& http_transaction,
                  InfiniteCacheTransaction* transaction) {
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(http_transaction.response_headers));
  net::HttpResponseInfo response;
  response.headers = headers;
  response.request_time = http_transaction.request_time.is_null() ?
                          Time::Now() : http_transaction.request_time;
  response.response_time = Time::Now();

  MockHttpRequest request(http_transaction);
  transaction->OnRequestStart(&request);
  transaction->OnResponseReceived(&response);
}

void ProcessRequest(const MockTransaction& http_transaction,
                    InfiniteCache* cache) {
  scoped_ptr<InfiniteCacheTransaction> transaction
      (cache->CreateInfiniteCacheTransaction());

  StartRequest(http_transaction, transaction.get());
  transaction->OnDataRead(http_transaction.data, strlen(http_transaction.data));
}

void ProcessRequestWithTime(const MockTransaction& http_transaction,
                            InfiniteCache* cache,
                            Time time) {
  scoped_ptr<InfiniteCacheTransaction> transaction
      (cache->CreateInfiniteCacheTransaction());

  MockTransaction timed_transaction = http_transaction;
  timed_transaction.request_time = time;
  StartRequest(timed_transaction, transaction.get());
  transaction->OnDataRead(http_transaction.data, strlen(http_transaction.data));
}

}  // namespace

TEST(InfiniteCache, Basics) {
  InfiniteCache cache;
  cache.Init(FilePath());

  scoped_ptr<InfiniteCacheTransaction> transaction
      (cache.CreateInfiniteCacheTransaction());

  // Don't even Start() this transaction.
  transaction.reset(cache.CreateInfiniteCacheTransaction());

  net::TestCompletionCallback cb;
  EXPECT_EQ(0, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  MockHttpRequest request(kTypicalGET_Transaction);
  transaction->OnRequestStart(&request);

  // Don't have a response yet.
  transaction.reset(cache.CreateInfiniteCacheTransaction());
  EXPECT_EQ(0, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  net::HttpResponseInfo response;
  scoped_refptr<net::HttpResponseHeaders> headers(
      new net::HttpResponseHeaders(kTypicalGET_Transaction.response_headers));
  response.headers = headers;

  transaction->OnRequestStart(&request);
  transaction->OnResponseReceived(&response);
  transaction.reset(cache.CreateInfiniteCacheTransaction());
  EXPECT_EQ(1, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  // Hit the same URL again.
  transaction->OnRequestStart(&request);
  transaction->OnResponseReceived(&response);
  transaction.reset(cache.CreateInfiniteCacheTransaction());
  EXPECT_EQ(1, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  // Now a different URL.
  MockHttpRequest request2(kSimpleGET_Transaction);
  transaction->OnRequestStart(&request2);
  transaction->OnResponseReceived(&response);
  transaction.reset();
  EXPECT_EQ(2, cb.GetResult(cache.QueryItemsForTest(cb.callback())));
}

TEST(InfiniteCache, Save_Restore) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.path().Append(FILE_PATH_LITERAL("infinite"));

  scoped_ptr<InfiniteCache> cache(new InfiniteCache);
  cache->Init(path);
  net::TestCompletionCallback cb;

  ProcessRequest(kTypicalGET_Transaction, cache.get());
  ProcessRequest(kSimpleGET_Transaction, cache.get());
  ProcessRequest(kETagGET_Transaction, cache.get());
  ProcessRequest(kSimplePOST_Transaction, cache.get());

  EXPECT_EQ(3, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));

  cache.reset(new InfiniteCache);
  cache->Init(path);
  EXPECT_EQ(3, cb.GetResult(cache->QueryItemsForTest(cb.callback())));

  ProcessRequest(kTypicalGET_Transaction, cache.get());
  EXPECT_EQ(3, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));
}

TEST(InfiniteCache, DoomMethod) {
  InfiniteCache cache;
  cache.Init(FilePath());

  ProcessRequest(kTypicalGET_Transaction, &cache);
  ProcessRequest(kSimpleGET_Transaction, &cache);
  ProcessRequest(kETagGET_Transaction, &cache);
  net::TestCompletionCallback cb;
  EXPECT_EQ(3, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  MockTransaction request(kTypicalGET_Transaction);
  request.method = "PUT";
  ProcessRequest(request, &cache);
  EXPECT_EQ(2, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  request.method = "POST";
  request.url = kSimpleGET_Transaction.url;
  ProcessRequest(request, &cache);
  EXPECT_EQ(1, cb.GetResult(cache.QueryItemsForTest(cb.callback())));

  request.method = "DELETE";
  request.url = kETagGET_Transaction.url;
  ProcessRequest(request, &cache);
  EXPECT_EQ(0, cb.GetResult(cache.QueryItemsForTest(cb.callback())));
}

TEST(InfiniteCache, Delete) {
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.path().Append(FILE_PATH_LITERAL("infinite"));

  scoped_ptr<InfiniteCache> cache(new InfiniteCache);
  cache->Init(path);
  net::TestCompletionCallback cb;

  ProcessRequest(kTypicalGET_Transaction, cache.get());
  ProcessRequest(kSimpleGET_Transaction, cache.get());
  EXPECT_EQ(2, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));
  EXPECT_TRUE(file_util::PathExists(path));

  cache.reset(new InfiniteCache);
  cache->Init(path);
  EXPECT_EQ(2, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_EQ(net::OK, cb.GetResult(cache->DeleteData(cb.callback())));
  EXPECT_EQ(0, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_FALSE(file_util::PathExists(path));

  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));
}

TEST(InfiniteCache, DeleteBetween) {
#if !defined(OS_ANDROID)
  ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  FilePath path = dir.path().Append(FILE_PATH_LITERAL("infinite"));

  scoped_ptr<InfiniteCache> cache(new InfiniteCache);
  cache->Init(path);
  net::TestCompletionCallback cb;

  Time::Exploded baseline = {};
  baseline.year = 2012;
  baseline.month = 1;
  baseline.day_of_month = 1;
  Time base_time = Time::FromUTCExploded(baseline);

  ProcessRequestWithTime(kTypicalGET_Transaction, cache.get(), base_time);

  Time start = base_time + TimeDelta::FromSeconds(2);
  ProcessRequestWithTime(kSimpleGET_Transaction, cache.get(), start);
  Time end = start + TimeDelta::FromSeconds(2);

  ProcessRequestWithTime(kETagGET_Transaction, cache.get(),
                          end + TimeDelta::FromSeconds(2));

  EXPECT_EQ(3, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  EXPECT_EQ(net::OK,
            cb.GetResult(cache->DeleteDataBetween(start, end,
                                                  cb.callback())));
  EXPECT_EQ(2, cb.GetResult(cache->QueryItemsForTest(cb.callback())));

  // Make sure the data is deleted from disk.
  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));
  cache.reset(new InfiniteCache);
  cache->Init(path);

  EXPECT_EQ(2, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  ProcessRequest(kETagGET_Transaction, cache.get());
  EXPECT_EQ(2, cb.GetResult(cache->QueryItemsForTest(cb.callback())));

  EXPECT_EQ(net::OK,
            cb.GetResult(cache->DeleteDataBetween(start, Time::Now(),
                                                  cb.callback())));
  EXPECT_EQ(1, cb.GetResult(cache->QueryItemsForTest(cb.callback())));

  // Make sure the data is deleted from disk.
  EXPECT_EQ(net::OK, cb.GetResult(cache->FlushDataForTest(cb.callback())));
  cache.reset(new InfiniteCache);
  cache->Init(path);

  EXPECT_EQ(1, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
  ProcessRequest(kTypicalGET_Transaction, cache.get());
  EXPECT_EQ(1, cb.GetResult(cache->QueryItemsForTest(cb.callback())));
#endif  // OS_ANDROID
}
