// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include "base/hash_tables.h"
#include "base/message_loop.h"
#include "base/scoped_vector.h"
#include "base/string_util.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/base/load_log_unittest.h"
#include "net/base/ssl_cert_request_info.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_byte_range.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;

namespace {

//-----------------------------------------------------------------------------
// mock disk cache (a very basic memory cache implementation)

class MockDiskEntry : public disk_cache::Entry,
                      public base::RefCounted<MockDiskEntry> {
 public:
  MockDiskEntry()
      : test_mode_(0), doomed_(false), sparse_(false), fail_requests_(false),
        busy_(false), delayed_(false) {
  }

  explicit MockDiskEntry(const std::string& key)
      : key_(key), doomed_(false), sparse_(false), fail_requests_(false),
        busy_(false), delayed_(false) {
    //
    // 'key' is prefixed with an identifier if it corresponds to a cached POST.
    // Skip past that to locate the actual URL.
    //
    // TODO(darin): It breaks the abstraction a bit that we assume 'key' is an
    // URL corresponding to a registered MockTransaction.  It would be good to
    // have another way to access the test_mode.
    //
    GURL url;
    if (isdigit(key[0])) {
      size_t slash = key.find('/');
      DCHECK(slash != std::string::npos);
      url = GURL(key.substr(slash + 1));
    } else {
      url = GURL(key);
    }
    const MockTransaction* t = FindMockTransaction(url);
    DCHECK(t);
    test_mode_ = t->test_mode;
  }

  bool is_doomed() const { return doomed_; }

  virtual void Doom() {
    doomed_ = true;
  }

  virtual void Close() {
    Release();
  }

  virtual std::string GetKey() const {
    if (fail_requests_)
      return std::string();
    return key_;
  }

  virtual Time GetLastUsed() const {
    return Time::FromInternalValue(0);
  }

  virtual Time GetLastModified() const {
    return Time::FromInternalValue(0);
  }

  virtual int32 GetDataSize(int index) const {
    DCHECK(index >= 0 && index < 2);
    return static_cast<int32>(data_[index].size());
  }

  virtual int ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                       net::CompletionCallback* callback) {
    DCHECK(index >= 0 && index < 2);

    if (fail_requests_)
      return net::ERR_CACHE_READ_FAILURE;

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;
    if (static_cast<size_t>(offset) == data_[index].size())
      return 0;

    int num = std::min(buf_len, static_cast<int>(data_[index].size()) - offset);
    memcpy(buf->data(), &data_[index][offset], num);

    if (!callback || (test_mode_ & TEST_MODE_SYNC_CACHE_READ))
      return num;

    CallbackLater(callback, num);
    return net::ERR_IO_PENDING;
  }

  virtual int WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* callback, bool truncate) {
    DCHECK(index >= 0 && index < 2);
    DCHECK(truncate);

    if (fail_requests_)
      return net::ERR_CACHE_READ_FAILURE;

    if (offset < 0 || offset > static_cast<int>(data_[index].size()))
      return net::ERR_FAILED;

    data_[index].resize(offset + buf_len);
    if (buf_len)
      memcpy(&data_[index][offset], buf->data(), buf_len);

    if (!callback || (test_mode_ & TEST_MODE_SYNC_CACHE_WRITE))
      return buf_len;

    CallbackLater(callback, buf_len);
    return net::ERR_IO_PENDING;
  }

  virtual int ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                             net::CompletionCallback* completion_callback) {
    if (!sparse_ || busy_)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    if (offset < 0)
      return net::ERR_FAILED;

    if (fail_requests_)
      return net::ERR_CACHE_READ_FAILURE;

    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);
    if (!buf_len)
      return 0;

    int num = std::min(static_cast<int>(data_[1].size()) - real_offset,
                       buf_len);
    memcpy(buf->data(), &data_[1][real_offset], num);

    if (!completion_callback || (test_mode_ & TEST_MODE_SYNC_CACHE_READ))
      return num;

    CallbackLater(completion_callback, num);
    busy_ = true;
    delayed_ = false;
    return net::ERR_IO_PENDING;
  }

  virtual int WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* completion_callback) {
    if (busy_)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    if (!sparse_) {
      if (data_[1].size())
        return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
      sparse_ = true;
    }
    if (offset < 0)
      return net::ERR_FAILED;
    if (!buf_len)
      return 0;

    if (fail_requests_)
      return net::ERR_CACHE_READ_FAILURE;

    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);

    if (static_cast<int>(data_[1].size()) < real_offset + buf_len)
      data_[1].resize(real_offset + buf_len);

    memcpy(&data_[1][real_offset], buf->data(), buf_len);
    if (!completion_callback || (test_mode_ & TEST_MODE_SYNC_CACHE_WRITE))
      return buf_len;

    CallbackLater(completion_callback, buf_len);
    return net::ERR_IO_PENDING;
  }

  virtual int GetAvailableRange(int64 offset, int len, int64* start) {
    if (!sparse_ || busy_)
      return net::ERR_CACHE_OPERATION_NOT_SUPPORTED;
    if (offset < 0)
      return net::ERR_FAILED;

    if (fail_requests_)
      return net::ERR_CACHE_READ_FAILURE;

    *start = offset;
    DCHECK(offset < kint32max);
    int real_offset = static_cast<int>(offset);
    if (static_cast<int>(data_[1].size()) < real_offset)
      return 0;

    int num = std::min(static_cast<int>(data_[1].size()) - real_offset, len);
    int count = 0;
    for (; num > 0; num--, real_offset++) {
      if (!count) {
        if (data_[1][real_offset]) {
          count++;
          *start = real_offset;
        }
      } else {
        if (!data_[1][real_offset])
          break;
        count++;
      }
    }
    return count;
  }

  virtual int GetAvailableRange(int64 offset, int len, int64* start,
                                net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual void CancelSparseIO() { cancel_ = true; }

  virtual int ReadyForSparseIO(net::CompletionCallback* completion_callback) {
    if (!cancel_)
      return net::OK;

    cancel_ = false;
    DCHECK(completion_callback);
    if (test_mode_ & TEST_MODE_SYNC_CACHE_READ)
      return net::OK;

    // The pending operation is already in the message loop (and hopefuly
    // already in the second pass).  Just notify the caller that it finished.
    CallbackLater(completion_callback, 0);
    return net::ERR_IO_PENDING;
  }

  // Fail most subsequent requests.
  void set_fail_requests() { fail_requests_ = true; }

 private:
  friend class base::RefCounted<MockDiskEntry>;

  ~MockDiskEntry() {}

  // Unlike the callbacks for MockHttpTransaction, we want this one to run even
  // if the consumer called Close on the MockDiskEntry.  We achieve that by
  // leveraging the fact that this class is reference counted.
  void CallbackLater(net::CompletionCallback* callback, int result) {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
        &MockDiskEntry::RunCallback, callback, result));
  }
  void RunCallback(net::CompletionCallback* callback, int result) {
    if (busy_) {
      // This is kind of hacky, but controlling the behavior of just this entry
      // from a test is sort of complicated.  What we really want to do is
      // delay the delivery of a sparse IO operation a little more so that the
      // request start operation (async) will finish without seeing the end of
      // this operation (already posted to the message loop)... and without
      // just delaying for n mS (which may cause trouble with slow bots).  So
      // we re-post this operation (all async sparse IO operations will take two
      // trips trhough the message loop instead of one).
      if (!delayed_) {
        delayed_ = true;
        return CallbackLater(callback, result);
      }
    }
    busy_ = false;
    callback->Run(result);
  }

  std::string key_;
  std::vector<char> data_[2];
  int test_mode_;
  bool doomed_;
  bool sparse_;
  bool fail_requests_;
  bool busy_;
  bool delayed_;
  static bool cancel_;
};

// Static.
bool MockDiskEntry::cancel_ = false;

class MockDiskCache : public disk_cache::Backend {
 public:
  MockDiskCache()
      : open_count_(0), create_count_(0), fail_requests_(false),
        soft_failures_(false) {
  }

  ~MockDiskCache() {
    EntryMap::iterator it = entries_.begin();
    for (; it != entries_.end(); ++it)
      it->second->Release();
  }

  virtual int32 GetEntryCount() const {
    return static_cast<int32>(entries_.size());
  }

  virtual bool OpenEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    if (it == entries_.end())
      return false;

    if (it->second->is_doomed()) {
      it->second->Release();
      entries_.erase(it);
      return false;
    }

    open_count_++;

    it->second->AddRef();
    *entry = it->second;

    if (soft_failures_)
      it->second->set_fail_requests();

    return true;
  }

  virtual int OpenEntry(const std::string& key, disk_cache::Entry** entry,
                        net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual bool CreateEntry(const std::string& key, disk_cache::Entry** entry) {
    if (fail_requests_)
      return false;

    EntryMap::iterator it = entries_.find(key);
    DCHECK(it == entries_.end());

    create_count_++;

    MockDiskEntry* new_entry = new MockDiskEntry(key);

    new_entry->AddRef();
    entries_[key] = new_entry;

    new_entry->AddRef();
    *entry = new_entry;

    if (soft_failures_)
      new_entry->set_fail_requests();

    return true;
  }

  virtual int CreateEntry(const std::string& key, disk_cache::Entry** entry,
                          net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual bool DoomEntry(const std::string& key) {
    EntryMap::iterator it = entries_.find(key);
    if (it != entries_.end()) {
      it->second->Release();
      entries_.erase(it);
    }
    return true;
  }

  virtual bool DoomAllEntries() {
    return false;
  }

  virtual int DoomAllEntries(net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual bool DoomEntriesBetween(const Time initial_time,
                                  const Time end_time) {
    return true;
  }

  virtual int DoomEntriesBetween(const base::Time initial_time,
                                 const base::Time end_time,
                                 net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual bool DoomEntriesSince(const Time initial_time) {
    return true;
  }

  virtual int DoomEntriesSince(const base::Time initial_time,
                               net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual bool OpenNextEntry(void** iter, disk_cache::Entry** next_entry) {
    return false;
  }

  virtual int OpenNextEntry(void** iter, disk_cache::Entry** next_entry,
                            net::CompletionCallback* callback) {
    return net::ERR_NOT_IMPLEMENTED;
  }

  virtual void EndEnumeration(void** iter) {}

  virtual void GetStats(
      std::vector<std::pair<std::string, std::string> >* stats) {
  }

  // returns number of times a cache entry was successfully opened
  int open_count() const { return open_count_; }

  // returns number of times a cache entry was successfully created
  int create_count() const { return create_count_; }

  // Fail any subsequent CreateEntry and OpenEntry.
  void set_fail_requests() { fail_requests_ = true; }

  // Return entries that fail some of their requests.
  void set_soft_failures(bool value) { soft_failures_ = value; }

 private:
  typedef base::hash_map<std::string, MockDiskEntry*> EntryMap;
  EntryMap entries_;
  int open_count_;
  int create_count_;
  bool fail_requests_;
  bool soft_failures_;
};

class MockHttpCache {
 public:
  MockHttpCache() : http_cache_(new MockNetworkLayer(), new MockDiskCache()) {
  }

  explicit MockHttpCache(disk_cache::Backend* disk_cache)
      : http_cache_(new MockNetworkLayer(), disk_cache) {
  }

  net::HttpCache* http_cache() { return &http_cache_; }

  MockNetworkLayer* network_layer() {
    return static_cast<MockNetworkLayer*>(http_cache_.network_layer());
  }
  MockDiskCache* disk_cache() {
    return static_cast<MockDiskCache*>(http_cache_.GetBackend());
  }

 private:
  net::HttpCache http_cache_;
};


//-----------------------------------------------------------------------------
// helpers

void ReadAndVerifyTransaction(net::HttpTransaction* trans,
                              const MockTransaction& trans_info) {
  std::string content;
  int rv = ReadTransaction(trans, &content);

  EXPECT_EQ(net::OK, rv);
  std::string expected(trans_info.data);
  EXPECT_EQ(expected, content);
}

void RunTransactionTestWithRequestAndLog(net::HttpCache* cache,
                                         const MockTransaction& trans_info,
                                         const MockHttpRequest& request,
                                         net::HttpResponseInfo* response_info,
                                         net::LoadLog* load_log) {
  TestCompletionCallback callback;

  // write to the cache

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, &callback, load_log);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  ASSERT_TRUE(response);

  if (response_info)
    *response_info = *response;

  ReadAndVerifyTransaction(trans.get(), trans_info);
}

void RunTransactionTestWithRequest(net::HttpCache* cache,
                                   const MockTransaction& trans_info,
                                   const MockHttpRequest& request,
                                   net::HttpResponseInfo* response_info) {
  RunTransactionTestWithRequestAndLog(cache, trans_info, request,
                                      response_info, NULL);
}

void RunTransactionTestWithLog(net::HttpCache* cache,
                               const MockTransaction& trans_info,
                               net::LoadLog* log) {
  RunTransactionTestWithRequestAndLog(
      cache, trans_info, MockHttpRequest(trans_info), NULL, log);
}

void RunTransactionTest(net::HttpCache* cache,
                        const MockTransaction& trans_info) {
  RunTransactionTestWithLog(cache, trans_info, NULL);
}

void RunTransactionTestWithResponseInfo(net::HttpCache* cache,
                                        const MockTransaction& trans_info,
                                        net::HttpResponseInfo* response) {
  RunTransactionTestWithRequest(
      cache, trans_info, MockHttpRequest(trans_info), response);
}

void RunTransactionTestWithResponse(net::HttpCache* cache,
                                    const MockTransaction& trans_info,
                                    std::string* response_headers) {
  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache, trans_info, &response);
  response.headers->GetNormalizedHeaders(response_headers);
}

// This class provides a handler for kFastNoStoreGET_Transaction so that the
// no-store header can be included on demand.
class FastTransactionServer {
 public:
  FastTransactionServer() {
    no_store = false;
  }
  ~FastTransactionServer() {}

  void set_no_store(bool value) { no_store = value; }

  static void FastNoStoreHandler(const net::HttpRequestInfo* request,
                                 std::string* response_status,
                                 std::string* response_headers,
                                 std::string* response_data) {
    if (no_store)
      *response_headers = "Cache-Control: no-store\n";
  }

 private:
  static bool no_store;
  DISALLOW_COPY_AND_ASSIGN(FastTransactionServer);
};
bool FastTransactionServer::no_store;

const MockTransaction kFastNoStoreGET_Transaction = {
  "http://www.google.com/nostore",
  "GET",
  base::Time(),
  "",
  net::LOAD_VALIDATE_CACHE,
  "HTTP/1.1 200 OK",
  "Cache-Control: max-age=10000\n",
  base::Time(),
  "<html><body>Google Blah Blah</body></html>",
  TEST_MODE_SYNC_NET_START,
  &FastTransactionServer::FastNoStoreHandler,
  0
};

// This class provides a handler for kRangeGET_TransactionOK so that the range
// request can be served on demand.
class RangeTransactionServer {
 public:
  RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
  }
  ~RangeTransactionServer() {
    not_modified_ = false;
    modified_ = false;
  }

  // Returns only 416 or 304 when set.
  void set_not_modified(bool value) { not_modified_ = value; }

  // Returns 206 when revalidating a range (instead of 304).
  void set_modified(bool value) { modified_ = value; }

  static void RangeHandler(const net::HttpRequestInfo* request,
                           std::string* response_status,
                           std::string* response_headers,
                           std::string* response_data);

 private:
  static bool not_modified_;
  static bool modified_;
  DISALLOW_COPY_AND_ASSIGN(RangeTransactionServer);
};
bool RangeTransactionServer::not_modified_ = false;
bool RangeTransactionServer::modified_ = false;

// A dummy extra header that must be preserved on a given request.
#define EXTRA_HEADER "Extra: header\r\n"

// Static.
void RangeTransactionServer::RangeHandler(const net::HttpRequestInfo* request,
                                          std::string* response_status,
                                          std::string* response_headers,
                                          std::string* response_data) {
  if (request->extra_headers.empty()) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    return;
  }

  // We want to make sure we don't delete extra headers.
  EXPECT_TRUE(request->extra_headers.find(EXTRA_HEADER) != std::string::npos);

  if (not_modified_) {
    response_status->assign("HTTP/1.1 304 Not Modified");
    return;
  }

  std::vector<net::HttpByteRange> ranges;
  if (!net::HttpUtil::ParseRanges(request->extra_headers, &ranges) ||
      ranges.size() != 1)
    return;
  // We can handle this range request.
  net::HttpByteRange byte_range = ranges[0];
  if (byte_range.first_byte_position() > 79) {
    response_status->assign("HTTP/1.1 416 Requested Range Not Satisfiable");
    return;
  }

  EXPECT_TRUE(byte_range.ComputeBounds(80));
  int start = static_cast<int>(byte_range.first_byte_position());
  int end = static_cast<int>(byte_range.last_byte_position());

  EXPECT_LT(end, 80);

  std::string content_range = StringPrintf("Content-Range: bytes %d-%d/80\n",
                                           start, end);
  response_headers->append(content_range);

  if (request->extra_headers.find("If-None-Match") == std::string::npos ||
      modified_) {
    EXPECT_EQ(9, (end - start) % 10);
    std::string data;
    for (int block_start = start; block_start < end; block_start += 10)
      StringAppendF(&data, "rg: %02d-%02d ", block_start, block_start + 9);
    *response_data = data;

    if (end - start != 9) {
      // We also have to fix content-length.
      int len = end - start + 1;
      EXPECT_EQ(0, len % 10);
      std::string content_length = StringPrintf("Content-Length: %d\n", len);
      response_headers->replace(response_headers->find("Content-Length:"),
                                content_length.size(), content_length);
    }
  } else {
    response_status->assign("HTTP/1.1 304 Not Modified");
    response_data->clear();
  }
}

const MockTransaction kRangeGET_TransactionOK = {
  "http://www.google.com/range",
  "GET",
  base::Time(),
  "Range: bytes = 40-49\r\n"
  EXTRA_HEADER,
  net::LOAD_NORMAL,
  "HTTP/1.1 206 Partial Content",
  "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
  "ETag: \"foo\"\n"
  "Accept-Ranges: bytes\n"
  "Content-Length: 10\n",
  base::Time(),
  "rg: 40-49 ",
  TEST_MODE_NORMAL,
  &RangeTransactionServer::RangeHandler,
  0
};

// Returns true if the response headers (|response|) match a partial content
// response for the range starting at |start| and ending at |end|.
bool Verify206Response(std::string response, int start, int end) {
  std::string raw_headers(net::HttpUtil::AssembleRawHeaders(response.data(),
                                                            response.size()));
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders(raw_headers);

  if (206 != headers->response_code())
    return false;

  int64 range_start, range_end, object_size;
  if (!headers->GetContentRange(&range_start, &range_end, &object_size))
    return false;
  int64 content_length = headers->GetContentLength();

  int length = end - start + 1;
  if (content_length != length)
    return false;

  if (range_start != start)
    return false;

  if (range_end != end)
    return false;

  return true;
}

// Helper to represent a network HTTP response.
struct Response {
  // Set this response into |trans|.
  void AssignTo(MockTransaction* trans) const {
    trans->status = status;
    trans->response_headers = headers;
    trans->data = body;
  }

  std::string status_and_headers() const {
    return std::string(status) + "\n" + std::string(headers);
  }

  const char* status;
  const char* headers;
  const char* body;
};

struct Context {
  Context() : result(net::ERR_IO_PENDING) {}

  int result;
  TestCompletionCallback callback;
  scoped_ptr<net::HttpTransaction> trans;
};

}  // namespace


//-----------------------------------------------------------------------------
// tests


TEST(HttpCache, CreateThenDestroy) {
  MockHttpCache cache;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());
}

TEST(HttpCache, GetBackend) {
  // This will initialize a cache object with NULL backend.
  MockHttpCache cache(NULL);

  // This will lazily initialize the backend.
  cache.http_cache()->set_type(net::MEMORY_CACHE);
  EXPECT_TRUE(cache.http_cache()->GetBackend());
}

TEST(HttpCache, SimpleGET) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGETNoDiskCache) {
  MockHttpCache cache;

  cache.disk_cache()->set_fail_requests();

  scoped_refptr<net::LoadLog> log(new net::LoadLog(net::LoadLog::kUnbounded));

  // Read from the network, and don't use the cache.
  RunTransactionTestWithLog(cache.http_cache(), kSimpleGET_Transaction, log);

  // Check that the LoadLog was filled as expected.
  // (We attempted to both Open and Create entries, but both failed).
  EXPECT_EQ(4u, log->events().size());
  net::ExpectLogContains(log, 0, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 1, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_END);
  net::ExpectLogContains(log, 2, net::LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 3, net::LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY,
                         net::LoadLog::PHASE_END);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGETWithDiskFailures) {
  MockHttpCache cache;

  cache.disk_cache()->set_soft_failures(true);

  // Read from the network, and fail to write to the cache.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that disk failures after the transaction has started don't cause the
// request to fail.
TEST(HttpCache, SimpleGETWithDiskFailures2) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  scoped_ptr<Context> c(new Context());
  int rv = cache.http_cache()->CreateTransaction(&c->trans);
  EXPECT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, &c->callback, NULL);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = c->callback.WaitForResult();

  // Start failing request now.
  cache.disk_cache()->set_soft_failures(true);

  // We have to open the entry again to propagate the failure flag.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.disk_cache()->OpenEntry(kSimpleGET_Transaction.url, &en));
  en->Close();

  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  c.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This one should see an empty cache again.
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  scoped_refptr<net::LoadLog> log(new net::LoadLog(net::LoadLog::kUnbounded));

  // write to the cache
  RunTransactionTestWithLog(cache.http_cache(), kSimpleGET_Transaction, log);

  // Check that the LoadLog was filled as expected.
  EXPECT_EQ(6u, log->events().size());
  net::ExpectLogContains(log, 0, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 1, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_END);
  net::ExpectLogContains(log, 2, net::LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 3, net::LoadLog::TYPE_HTTP_CACHE_CREATE_ENTRY,
                         net::LoadLog::PHASE_END);
  net::ExpectLogContains(log, 4, net::LoadLog::TYPE_HTTP_CACHE_WAITING,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 5, net::LoadLog::TYPE_HTTP_CACHE_WAITING,
                         net::LoadLog::PHASE_END);

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  log = new net::LoadLog(net::LoadLog::kUnbounded);

  RunTransactionTestWithLog(cache.http_cache(), transaction, log);

  // Check that the LoadLog was filled as expected.
  EXPECT_EQ(6u, log->events().size());
  net::ExpectLogContains(log, 0, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 1, net::LoadLog::TYPE_HTTP_CACHE_OPEN_ENTRY,
                         net::LoadLog::PHASE_END);
  net::ExpectLogContains(log, 2, net::LoadLog::TYPE_HTTP_CACHE_WAITING,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 3, net::LoadLog::TYPE_HTTP_CACHE_WAITING,
                         net::LoadLog::PHASE_END);
  net::ExpectLogContains(log, 4, net::LoadLog::TYPE_HTTP_CACHE_READ_INFO,
                         net::LoadLog::PHASE_BEGIN);
  net::ExpectLogContains(log, 5, net::LoadLog::TYPE_HTTP_CACHE_READ_INFO,
                         net::LoadLog::PHASE_END);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, &callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Hit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadPreferringCache_Miss) {
  MockHttpCache cache;

  // force this transaction to read from the cache if valid
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_PREFERRING_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_BYPASS_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "pragma: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadBypassCache_Implicit2) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: no-cache";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimpleGET_LoadValidateCache_Implicit) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "cache-control: max-age=0";

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void PreserveRequestHeaders_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find(EXTRA_HEADER) != std::string::npos);
}

// Tests that we don't remove extra headers for simple requests.
TEST(HttpCache, SimpleGET_PreserveRequestHeaders) {
  MockHttpCache cache;

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.handler = PreserveRequestHeaders_Handler;
  transaction.request_headers = EXTRA_HEADER;
  transaction.response_headers = "Cache-Control: max-age=0\n";
  AddMockTransaction(&transaction);

  // Write, then revalidate the entry.
  RunTransactionTest(cache.http_cache(), transaction);
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we don't remove extra headers for conditionalized requests.
TEST(HttpCache, ConditionalizedGET_PreserveRequestHeaders) {
  MockHttpCache cache;

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  MockTransaction transaction(kETagGET_Transaction);
  transaction.handler = PreserveRequestHeaders_Handler;
  transaction.request_headers = "If-None-Match: \"foopy\"\n"
                                EXTRA_HEADER;
  AddMockTransaction(&transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

TEST(HttpCache, SimpleGET_ManyReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.http_cache()->CreateTransaction(&c->trans);
    EXPECT_EQ(net::OK, c->result);

    c->result = c->trans->Start(&request, &c->callback, NULL);
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should not have had to re-open the disk entry

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4769.
// If cancelling a request is racing with another request for the same resource
// finishing, we have to make sure that we remove both transactions from the
// entry.
TEST(HttpCache, SimpleGET_RacingReaders) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest reader_request(kSimpleGET_Transaction);
  reader_request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  std::vector<Context*> context_list;
  const int kNumTransactions = 5;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.http_cache()->CreateTransaction(&c->trans);
    EXPECT_EQ(net::OK, c->result);

    MockHttpRequest* this_request = &request;
    if (i == 1 || i == 2)
      this_request = &reader_request;

    c->result = c->trans->Start(this_request, &c->callback, NULL);
  }

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  Context* c = context_list[0];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // Now we have 2 active readers and two queued transactions.

  c = context_list[1];
  ASSERT_EQ(net::ERR_IO_PENDING, c->result);
  c->result = c->callback.WaitForResult();
  if (c->result == net::OK)
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);

  // At this point we have one reader, two pending transactions and a task on
  // the queue to move to the next transaction. Now we cancel the request that
  // is the current reader, and expect the queued task to be able to start the
  // next request.

  c = context_list[2];
  c->trans.reset();

  for (int i = 3; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    if (c->result == net::OK)
      ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // We should not have had to re-open the disk entry.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

// Tests that we can doom an entry with pending transactions and delete one of
// the pending transactions before the first one completes.
// See http://code.google.com/p/chromium/issues/detail?id=25588
TEST(HttpCache, SimpleGET_DoomWithPending) {
  // We need simultaneous doomed / not_doomed entries so let's use a real cache.
  disk_cache::Backend* disk_cache =
      disk_cache::CreateInMemoryCacheBackend(1024 * 1024);
  MockHttpCache cache(disk_cache);

  MockHttpRequest request(kSimpleGET_Transaction);
  MockHttpRequest writer_request(kSimpleGET_Transaction);
  writer_request.load_flags = net::LOAD_BYPASS_CACHE;

  ScopedVector<Context> context_list;
  const int kNumTransactions = 4;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.http_cache()->CreateTransaction(&c->trans);
    EXPECT_EQ(net::OK, c->result);

    MockHttpRequest* this_request = &request;
    if (i == 3)
      this_request = &writer_request;

    c->result = c->trans->Start(this_request, &c->callback, NULL);
  }

  // The first request should be a writer at this point, and the two subsequent
  // requests should be pending. The last request doomed the first entry.

  EXPECT_EQ(2, cache.network_layer()->transaction_count());

  // Cancel the first queued transaction.
  delete context_list[1];
  context_list.get()[1] = NULL;

  for (int i = 0; i < kNumTransactions; ++i) {
    if (i == 1)
      continue;
    Context* c = context_list[i];
    ASSERT_EQ(net::ERR_IO_PENDING, c->result);
    c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }
}

// This is a test for http://code.google.com/p/chromium/issues/detail?id=4731.
// We may attempt to delete an entry synchronously with the act of adding a new
// transaction to said entry.
TEST(HttpCache, FastNoStoreGET_DoneWithPending) {
  MockHttpCache cache;

  // The headers will be served right from the call to Start() the request.
  MockHttpRequest request(kFastNoStoreGET_Transaction);
  FastTransactionServer request_handler;
  AddMockTransaction(&kFastNoStoreGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 3;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.http_cache()->CreateTransaction(&c->trans);
    EXPECT_EQ(net::OK, c->result);

    c->result = c->trans->Start(&request, &c->callback, NULL);
  }

  // The first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now, make sure that the second request asks for the entry not to be stored.
  request_handler.set_no_store(true);

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    ReadAndVerifyTransaction(c->trans.get(), kFastNoStoreGET_Transaction);
    delete c;
  }

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kFastNoStoreGET_Transaction);
}

TEST(HttpCache, SimpleGET_ManyWriters_CancelFirst) {
  MockHttpCache cache;

  MockHttpRequest request(kSimpleGET_Transaction);

  std::vector<Context*> context_list;
  const int kNumTransactions = 2;

  for (int i = 0; i < kNumTransactions; ++i) {
    context_list.push_back(new Context());
    Context* c = context_list[i];

    c->result = cache.http_cache()->CreateTransaction(&c->trans);
    EXPECT_EQ(net::OK, c->result);

    c->result = c->trans->Start(&request, &c->callback, NULL);
  }

  // the first request should be a writer at this point, and the subsequent
  // requests should be pending.

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  for (int i = 0; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    if (c->result == net::ERR_IO_PENDING)
      c->result = c->callback.WaitForResult();
    // destroy only the first transaction
    if (i == 0) {
      delete c;
      context_list[i] = NULL;
    }
  }

  // complete the rest of the transactions
  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    ReadAndVerifyTransaction(c->trans.get(), kSimpleGET_Transaction);
  }

  // we should have had to re-open the disk entry

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  for (int i = 1; i < kNumTransactions; ++i) {
    Context* c = context_list[i];
    delete c;
  }
}

TEST(HttpCache, SimpleGET_AbandonedCacheRead) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  MockHttpRequest request(kSimpleGET_Transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  rv = trans->Start(&request, &callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::OK, rv);

  scoped_refptr<net::IOBuffer> buf = new net::IOBuffer(256);
  rv = trans->Read(buf, 256, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Test that destroying the transaction while it is reading from the cache
  // works properly.
  trans.reset();

  // Make sure we pump any pending events, which should include a call to
  // HttpCache::Transaction::OnCacheReadCompleted.
  MessageLoop::current()->RunAllPending();
}

TEST(HttpCache, TypicalGET_ConditionalRequest) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  RunTransactionTest(cache.http_cache(), kTypicalGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find("If-None-Match") !=
                  std::string::npos);
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign(kETagGET_Transaction.response_headers);
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

static void ETagGet_ConditionalRequest_NoStore_Handler(
    const net::HttpRequestInfo* request,
    std::string* response_status,
    std::string* response_headers,
    std::string* response_data) {
  EXPECT_TRUE(request->extra_headers.find("If-None-Match") !=
              std::string::npos);
  response_status->assign("HTTP/1.1 304 Not Modified");
  response_headers->assign("Cache-Control: no-store\n");
  response_data->clear();
}

TEST(HttpCache, ETagGET_ConditionalRequest_304_NoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // Write to the cache.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Get the same URL again, but this time we expect it to result
  // in a conditional request.
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.handler = ETagGet_ConditionalRequest_NoStore_Handler;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  ScopedMockTransaction transaction2(kETagGET_Transaction);

  // Write to the cache again. This should create a new entry.
  RunTransactionTest(cache.http_cache(), transaction2);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests that do not have an upload
  // identifier.

  RunTransactionTest(cache.http_cache(), kSimplePOST_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Helper that does 4 requests using HttpCache:
//
// (1) loads |kUrl| -- expects |net_response_1| to be returned.
// (2) loads |kUrl| from cache only -- expects |net_response_1| to be returned.
// (3) loads |kUrl| using |extra_request_headers| -- expects |net_response_2| to
//     be returned.
// (4) loads |kUrl| from cache only -- expects |cached_response_2| to be
//     returned.
static void ConditionalizedRequestUpdatesCacheHelper(
    const Response& net_response_1,
    const Response& net_response_2,
    const Response& cached_response_2,
    const char* extra_request_headers) {
  MockHttpCache cache;

  // The URL we will be requesting.
  const char* kUrl = "http://foobar.com/main.css";

  // Junk network response.
  static const Response kUnexpectedResponse = {
    "HTTP/1.1 500 Unexpected",
    "Server: unexpected_header",
    "unexpected body"
  };

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  // Request |kUrl| for the first time. It should hit the network and
  // receive |kNetResponse1|, which it saves into the HTTP cache.

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "";

  net_response_1.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| a second time. Now |kNetResponse1| it is in the HTTP
  // cache, so we don't hit the network.

  request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  net_response_1.AssignTo(&request);                     // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_1.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request |kUrl| yet again, but this time give the request an
  // "If-Modified-Since" header. This will cause the request to re-hit the
  // network. However now the network response is going to be
  // different -- this simulates a change made to the CSS file.

  request.request_headers = extra_request_headers;
  request.load_flags = net::LOAD_NORMAL;

  net_response_2.AssignTo(&mock_network_response);  // Network mock.
  net_response_2.AssignTo(&request);                // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(net_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Finally, request |kUrl| again. This request should be serviced from
  // the cache. Moreover, the value in the cache should be |kNetResponse2|
  // and NOT |kNetResponse1|. The previous step should have replaced the
  // value in the cache with the modified response.

  request.request_headers = "";
  request.load_flags = net::LOAD_ONLY_FROM_CACHE;

  kUnexpectedResponse.AssignTo(&mock_network_response);  // Network mock.
  cached_response_2.AssignTo(&request);                  // Expected result.

  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(cached_response_2.status_and_headers(), response_headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Check that when an "if-modified-since" header is attached
// to the request, the result still updates the cached entry.
TEST(HttpCache, ConditionalizedRequestUpdatesCache1) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  const char* extra_headers =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, extra_headers);
}

// Check that when an "if-none-match" header is attached
// to the request, the result updates the cached entry.
TEST(HttpCache, ConditionalizedRequestUpdatesCache2) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"ETAG1\"\n"
    "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"ETAG2\"\n"
    "Expires: Wed, 7 Sep 2033 21:46:42 GMT\n",  // Should never expire.
    "body2"
  };

  const char* extra_headers = "If-None-Match: \"ETAG1\"\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, extra_headers);
}

// Check that when an "if-modified-since" header is attached
// to a request, the 304 (not modified result) result updates the cached
// headers, and the 304 response is returned rather than the cached response.
TEST(HttpCache, ConditionalizedRequestUpdatesCache3) {
  // First network response for |kUrl|.
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Server: server1\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  static const Response kCachedResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  const char* extra_headers =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kCachedResponse2, extra_headers);
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (304 response).
TEST(HttpCache, ConditionalizedRequestUpdatesCache4) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar.com/main.css";

  static const Response kNetResponse = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Test that when doing an externally conditionalized if-modified-since
// and there is no corresponding cache entry, a new cache entry is NOT
// created (200 response).
TEST(HttpCache, ConditionalizedRequestUpdatesCache5) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar.com/main.css";

  static const Response kNetResponse = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "foobar!!!"
  };

  const char* kExtraRequestHeaders =
      "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n";

  // We will control the network layer's responses for |kUrl| using
  // |mock_network_response|.
  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;
  AddMockTransaction(&mock_network_response);

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = kExtraRequestHeaders;

  kNetResponse.AssignTo(&mock_network_response);  // Network mock.
  kNetResponse.AssignTo(&request);                // Expected result.

  std::string response_headers;
  RunTransactionTestWithResponse(
      cache.http_cache(), request, &response_headers);

  EXPECT_EQ(kNetResponse.status_and_headers(), response_headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&mock_network_response);
}

// Test that when doing an externally conditionalized if-modified-since
// if the date does not match the cache entry's last-modified date,
// then we do NOT use the response (304) to update the cache.
// (the if-modified-since date is 2 days AFTER the cache's modification date).
TEST(HttpCache, ConditionalizedRequestUpdatesCache6) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Server: server1\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Server: server2\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  // This is two days in the future from the original response's last-modified
  // date!
  const char* kExtraRequestHeaders =
      "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that when doing an externally conditionalized if-none-match
// if the etag does not match the cache entry's etag, then we do not use the
// response (304) to update the cache.
TEST(HttpCache, ConditionalizedRequestUpdatesCache7) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    ""
  };

  // Different etag from original response.
  const char* kExtraRequestHeaders = "If-None-Match: \"Foo2\"\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since updates the cache.
TEST(HttpCache, ConditionalizedRequestUpdatesCache8) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  const char* kExtraRequestHeaders =
    "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n"
    "If-None-Match: \"Foo1\"\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse2, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST(HttpCache, ConditionalizedRequestUpdatesCache9) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  // The etag doesn't match what we have stored.
  const char* kExtraRequestHeaders =
    "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n"
    "If-None-Match: \"Foo2\"\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with both if-none-match
// and if-modified-since does not update the cache with only one match.
TEST(HttpCache, ConditionalizedRequestUpdatesCache10) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  // The modification date doesn't match what we have stored.
  const char* kExtraRequestHeaders =
    "If-Modified-Since: Fri, 08 Feb 2008 22:38:21 GMT\n"
    "If-None-Match: \"Foo1\"\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

// Test that doing an externally conditionalized request with two conflicting
// headers does not update the cache.
TEST(HttpCache, ConditionalizedRequestUpdatesCache11) {
  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Etag: \"Foo1\"\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    "body1"
  };

  // Second network response for |kUrl|.
  static const Response kNetResponse2 = {
    "HTTP/1.1 200 OK",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
    "Etag: \"Foo2\"\n"
    "Last-Modified: Fri, 03 Jul 2009 02:14:27 GMT\n",
    "body2"
  };

  // Two dates, the second matches what we have stored.
  const char* kExtraRequestHeaders =
    "If-Modified-Since: Mon, 04 Feb 2008 22:38:21 GMT\n"
    "If-Modified-Since: Wed, 06 Feb 2008 22:38:21 GMT\n";

  ConditionalizedRequestUpdatesCacheHelper(
      kNetResponse1, kNetResponse2, kNetResponse1, kExtraRequestHeaders);
}

TEST(HttpCache, UrlContainingHash) {
  MockHttpCache cache;

  // Do a typical GET request -- should write an entry into our cache.
  MockTransaction trans(kTypicalGET_Transaction);
  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Request the same URL, but this time with a reference section (hash).
  // Since the cache key strips the hash sections, this should be a cache hit.
  std::string url_with_hash = std::string(trans.url) + "#multiple#hashes";
  trans.url = url_with_hash.c_str();
  trans.load_flags = net::LOAD_ONLY_FROM_CACHE;

  RunTransactionTest(cache.http_cache(), trans);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Miss) {
  MockHttpCache cache;

  // Test that we skip the cache for POST requests.  Eventually, we will want
  // to cache these, but we'll still have cases where skipping the cache makes
  // sense, so we want to make sure that it works properly.

  MockTransaction transaction(kSimplePOST_Transaction);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, &callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(0, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

TEST(HttpCache, SimplePOST_LoadOnlyFromCache_Hit) {
  MockHttpCache cache;

  // Test that we hit the cache for POST requests.

  MockTransaction transaction(kSimplePOST_Transaction);

  const int64 kUploadId = 1;  // Just a dummy value.

  MockHttpRequest request(transaction);
  request.upload_data = new net::UploadData();
  request.upload_data->set_identifier(kUploadId);
  request.upload_data->AppendBytes("hello", 5);

  // Populate the cache.
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Load from cache.
  request.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  RunTransactionTestWithRequest(cache.http_cache(), transaction, request, NULL);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, RangeGET_SkipsCache) {
  MockHttpCache cache;

  // Test that we skip the cache for range GET requests.  Eventually, we will
  // want to cache these, but we'll still have cases where skipping the cache
  // makes sense, so we want to make sure that it works properly.

  RunTransactionTest(cache.http_cache(), kRangeGET_Transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  MockTransaction transaction(kSimpleGET_Transaction);
  transaction.request_headers = "If-None-Match: foo";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Test that we skip the cache for range requests that include a validation
// header.
TEST(HttpCache, RangeGET_SkipsCache2) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  MockTransaction transaction(kRangeGET_Transaction);
  transaction.request_headers = "If-None-Match: foo\n"
                                EXTRA_HEADER
                                "Range: bytes = 40-49\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers =
      "If-Modified-Since: Wed, 28 Nov 2007 00:45:20 GMT\n"
      EXTRA_HEADER
      "Range: bytes = 40-49\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  transaction.request_headers = "If-Range: bla\n"
                                EXTRA_HEADER
                                "Range: bytes = 40-49\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());
}

// Tests that receiving 206 for a regular request is handled correctly.
TEST(HttpCache, GET_Crazy206) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  // Write to the cache.
  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);
  transaction.request_headers = EXTRA_HEADER;
  transaction.handler = NULL;
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // This should read again from the net.
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
  RemoveMockTransaction(&transaction);
}

// Tests that we can cache range requests and fetch random blocks from the
// cache and the network.
TEST(HttpCache, RangeGET_OK) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write to the cache (30-39).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 30-39\r\n" EXTRA_HEADER;
  transaction.data = "rg: 30-39 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 30, 39));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (20-59).
  transaction.request_headers = "Range: bytes = 20-59\r\n" EXTRA_HEADER;
  transaction.data = "rg: 20-29 rg: 30-39 rg: 40-49 rg: 50-59 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 20, 59));
  EXPECT_EQ(5, cache.network_layer()->transaction_count());
  EXPECT_EQ(3, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we deal with 304s for range requests.
TEST(HttpCache, RangeGET_304) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we deal with 206s when revalidating range requests.
TEST(HttpCache, RangeGET_ModifiedResult) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Attempt to read from the cache (40-49).
  RangeTransactionServer handler;
  handler.set_modified(true);
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // And the entry should be gone.
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can cache range requests when the start or end is unknown.
// We start with one suffix request, followed by a request from a given point.
TEST(HttpCache, UnknownRangeGET_1) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = 60-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 60, 79));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can cache range requests when the start or end is unknown.
// We start with one request from a given point, followed by a suffix request.
// We'll also verify that synchronous cache responses work as intended.
TEST(HttpCache, UnknownRangeGET_2) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.test_mode = TEST_MODE_SYNC_CACHE_START |
                          TEST_MODE_SYNC_CACHE_READ |
                          TEST_MODE_SYNC_CACHE_WRITE;
  AddMockTransaction(&transaction);

  // Write to the cache (70-79).
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure we are done with the previous transaction.
  MessageLoop::current()->RunAllPending();

  // Write and read from the cache (60-79).
  transaction.request_headers = "Range: bytes = -20\r\n" EXTRA_HEADER;
  transaction.data = "rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 60, 79));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that receiving Not Modified when asking for an open range doesn't mess
// up things.
TEST(HttpCache, UnknownRangeGET_304) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  AddMockTransaction(&transaction);

  RangeTransactionServer handler;
  handler.set_not_modified(true);

  // Ask for the end of the file, without knowing the length.
  transaction.request_headers = "Range: bytes = 70-\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We just bypass the cache.
  EXPECT_EQ(0U, headers.find("HTTP/1.1 304 Not Modified\n"));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can handle non-range requests when we have cached a range.
TEST(HttpCache, GET_Previous206) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Write and read from the cache (0-79), when not asked for a range.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle non-range requests when we have cached the first
// part of the object and server replies with 304 (Not Modified).
TEST(HttpCache, GET_Previous206_NotModified) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 0-9\r\n" EXTRA_HEADER;
  transaction.data = "rg: 00-09 ";
  AddMockTransaction(&transaction);
  std::string headers;

  // Write to the cache (0-9).
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 0, 9));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Read from the cache (0-9), write and read from cache (10 - 79),
  MockTransaction transaction2(kRangeGET_TransactionOK);
  transaction2.request_headers = "Foo: bar\r\n" EXTRA_HEADER;
  transaction2.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                      "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 200 OK\n"));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
}

// Tests that we can handle cached 206 responses that are not sparse.
TEST(HttpCache, GET_Previous206_NotSparse) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  // Create a disk cache entry that stores 206 headers while not being sparse.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->CreateEntry(kSimpleGET_Transaction.url,
                                              &entry));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, false));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(500));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           kRangeGET_TransactionOK.data, 500));
  EXPECT_EQ(len, entry->WriteData(1, 0, buf, len, NULL, true));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kSimpleGET_Transaction,
                                 &headers);

  // We are expecting a 200.
  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we can handle cached 206 responses that are not sparse. This time
// we issue a range request and expect to receive a range.
TEST(HttpCache, RangeGET_Previous206_NotSparse_2) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Create a disk cache entry that stores 206 headers while not being sparse.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->CreateEntry(kRangeGET_TransactionOK.url,
                                              &entry));

  std::string raw_headers(kRangeGET_TransactionOK.status);
  raw_headers.append("\n");
  raw_headers.append(kRangeGET_TransactionOK.response_headers);
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, false));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(500));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           kRangeGET_TransactionOK.data, 500));
  EXPECT_EQ(len, entry->WriteData(1, 0, buf, len, NULL, true));
  entry->Close();

  // Now see that we don't use the stored entry.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  // We are expecting a 206.
  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle range requests with cached 200 responses.
TEST(HttpCache, RangeGET_Previous200) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  // Store the whole thing with status 200.
  MockTransaction transaction(kTypicalGET_Transaction);
  transaction.url = kRangeGET_TransactionOK.url;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  AddMockTransaction(&transaction);
  RunTransactionTest(cache.http_cache(), transaction);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Now see that we use the stored entry.
  std::string headers;
  MockTransaction transaction2(kRangeGET_TransactionOK);
  RangeTransactionServer handler;
  handler.set_not_modified(true);
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  // We are expecting a 206.
  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // The last transaction has finished so make sure the entry is deactivated.
  MessageLoop::current()->RunAllPending();

  // Now we should receive a range from the server and drop the stored entry.
  handler.set_not_modified(false);
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);
  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), transaction2);
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we can handle a 200 response when dealing with sparse entries.
TEST(HttpCache, RangeRequestResultsIn200) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (70-79).
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.data = "rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Now we'll issue a request that results in a plain 200 response, but to
  // the to the same URL that we used to store sparse data, and making sure
  // that we ask for a range.
  RemoveMockTransaction(&kRangeGET_TransactionOK);
  MockTransaction transaction2(kSimpleGET_Transaction);
  transaction2.url = kRangeGET_TransactionOK.url;
  transaction2.request_headers = kRangeGET_TransactionOK.request_headers;
  AddMockTransaction(&transaction2);

  RunTransactionTestWithResponse(cache.http_cache(), transaction2, &headers);

  std::string expected_headers(kSimpleGET_Transaction.status);
  expected_headers.append("\n");
  expected_headers.append(kSimpleGET_Transaction.response_headers);
  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction2);
}

// Tests that a range request that falls outside of the size that we know about
// only deletes the entry if the resource has indeed changed.
TEST(HttpCache, RangeGET_MoreThanCurrentSize) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);
  std::string headers;

  // Write to the cache (40-49).
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // A weird request should not delete this entry. Ask for bytes 120-.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = 120-\r\n" EXTRA_HEADER;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_EQ(0U, headers.find("HTTP/1.1 416 "));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(2, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't delete a sparse entry when we cancel a request.
TEST(HttpCache, RangeGET_Cancel) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  MockHttpRequest request(kRangeGET_TransactionOK);

  Context* c = new Context();
  int rv = cache.http_cache()->CreateTransaction(&c->trans);
  EXPECT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, &c->callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf = new net::IOBufferWithSize(10);
  rv = c->trans->Read(buf, buf->size(), &c->callback);
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  delete c;

  // Verify that the entry has not been deleted.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->OpenEntry(kRangeGET_TransactionOK.url,
                                            &entry));
  entry->Close();
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't delete a sparse entry when we start a new request after
// cancelling the previous one.
TEST(HttpCache, RangeGET_Cancel2) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  MockHttpRequest request(kRangeGET_TransactionOK);

  Context* c = new Context();
  int rv = cache.http_cache()->CreateTransaction(&c->trans);
  EXPECT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, &c->callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that we revalidate the entry and read from the cache (a single
  // read will return while waiting for the network).
  scoped_refptr<net::IOBufferWithSize> buf = new net::IOBufferWithSize(5);
  rv = c->trans->Read(buf, buf->size(), &c->callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = c->callback.WaitForResult();
  rv = c->trans->Read(buf, buf->size(), &c->callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  // Destroy the transaction before completing the read.
  delete c;

  // We have the read and the delete (OnProcessPendingQueue) waiting on the
  // message loop. This means that a new transaction will just reuse the same
  // active entry (no open or create).

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);

  EXPECT_EQ(3, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that an invalid range response results in no cached entry.
TEST(HttpCache, RangeGET_InvalidResponse1) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.response_headers = "Content-Range: bytes 40-49/45\n"
                                 "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* en;
  ASSERT_FALSE(cache.disk_cache()->OpenEntry(kRangeGET_TransactionOK.url, &en));

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we reject a range that doesn't match the content-length.
TEST(HttpCache, RangeGET_InvalidResponse2) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.response_headers = "Content-Range: bytes 40-49/80\n"
                                 "Content-Length: 20\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we don't have a cached entry.
  disk_cache::Entry* en;
  ASSERT_FALSE(cache.disk_cache()->OpenEntry(kRangeGET_TransactionOK.url, &en));

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that if a server tells us conflicting information about a resource we
// ignore the response.
TEST(HttpCache, RangeGET_InvalidResponse3) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.request_headers = "Range: bytes = 50-59\r\n" EXTRA_HEADER;
  std::string response_headers(transaction.response_headers);
  response_headers.append("Content-Range: bytes 50-59/160\n");
  transaction.response_headers = response_headers.c_str();
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 50, 59));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&transaction);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // This transaction will report a resource size of 80 bytes, and we think it's
  // 160 so we should ignore the response.
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Verify that we cached the first response but not the second one.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.disk_cache()->OpenEntry(kRangeGET_TransactionOK.url, &en));
  int64 cached_start = 0;
  EXPECT_EQ(10, en->GetAvailableRange(40, 20, &cached_start));
  EXPECT_EQ(50, cached_start);
  en->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we handle large range values properly.
TEST(HttpCache, RangeGET_LargeValues) {
  // We need a real sparse cache for this test.
  disk_cache::Backend* disk_cache =
      disk_cache::CreateInMemoryCacheBackend(1024 * 1024);
  MockHttpCache cache(disk_cache);
  cache.http_cache()->set_enable_range_support(true);
  std::string headers;

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.handler = NULL;
  transaction.request_headers = "Range: bytes = 4294967288-4294967297\r\n"
                                EXTRA_HEADER;
  transaction.response_headers =
      "Content-Range: bytes 4294967288-4294967297/4294967299\n"
      "Content-Length: 10\n";
  AddMockTransaction(&transaction);
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  std::string expected(transaction.status);
  expected.append("\n");
  expected.append(transaction.response_headers);
  EXPECT_EQ(expected, headers);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  // Verify that we have a cached entry.
  disk_cache::Entry* en;
  ASSERT_TRUE(cache.disk_cache()->OpenEntry(kRangeGET_TransactionOK.url, &en));
  en->Close();

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we don't crash with a range request if the disk cache was not
// initialized properly.
TEST(HttpCache, RangeGET_NoDiskCache) {
  MockHttpCache cache(NULL);
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

// Tests that we handle byte range requests that skip the cache.
TEST(HttpCache, RangeHEAD) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = "Range: bytes = -10\r\n" EXTRA_HEADER;
  transaction.method = "HEAD";
  transaction.data = "rg: 70-79 ";

  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  EXPECT_TRUE(Verify206Response(headers, 70, 79));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(0, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

#ifdef NDEBUG
// This test hits a NOTREACHED so it is a release mode only test.
TEST(HttpCache, RangeGET_OK_LoadOnlyFromCache) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Write to the cache (40-49).
  RunTransactionTest(cache.http_cache(), kRangeGET_TransactionOK);
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Force this transaction to read from the cache.
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, &callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);

  trans.reset();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}
#endif

// Tests the handling of the "truncation" flag.
TEST(HttpCache, WriteResponseInfo_Truncated) {
  MockHttpCache cache;
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->CreateEntry("http://www.google.com", &entry));

  std::string headers("HTTP/1.1 200 OK");
  headers = net::HttpUtil::AssembleRawHeaders(headers.data(), headers.size());
  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(headers);

  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, true));
  bool truncated = false;
  EXPECT_TRUE(net::HttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);

  // And now test the opposite case.
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, false));
  truncated = true;
  EXPECT_TRUE(net::HttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();
}

// Tests that we delete an entry when the request is cancelled before starting
// to read from the network.
TEST(HttpCache, DoomOnDestruction) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  MockHttpRequest request(kSimpleGET_Transaction);

  Context* c = new Context();
  int rv = cache.http_cache()->CreateTransaction(&c->trans);
  EXPECT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, &c->callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    c->result = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Destroy the transaction. We only have the headers so we should delete this
  // entry.
  delete c;

  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());
}

// Tests that we mark an entry as incomplete when the request is cancelled.
TEST(HttpCache, Set_Truncated_Flag) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);

  MockHttpRequest request(kSimpleGET_Transaction);

  Context* c = new Context();
  int rv = cache.http_cache()->CreateTransaction(&c->trans);
  EXPECT_EQ(net::OK, rv);

  rv = c->trans->Start(&request, &c->callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // Make sure that the entry has some data stored.
  scoped_refptr<net::IOBufferWithSize> buf = new net::IOBufferWithSize(10);
  rv = c->trans->Read(buf, buf->size(), &c->callback);
  if (rv == net::ERR_IO_PENDING)
    rv = c->callback.WaitForResult();
  EXPECT_EQ(buf->size(), rv);

  // Destroy the transaction.
  delete c;

  // Verify that the entry is marked as incomplete.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->OpenEntry(kSimpleGET_Transaction.url,
                                            &entry));
  net::HttpResponseInfo response;
  bool truncated = false;
  EXPECT_TRUE(net::HttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_TRUE(truncated);
  entry->Close();
}

// Tests that we can continue with a request that was interrupted.
TEST(HttpCache, GET_IncompleteResource) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Create a disk cache entry that stores an incomplete resource.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->CreateEntry(kRangeGET_TransactionOK.url,
                                              &entry));

  // Content-length will be intentionally bogus.
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: something\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 10\n");
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, true));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(100));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           "rg: 00-09 rg: 10-19 ", 100));
  EXPECT_EQ(len, entry->WriteData(1, 0, buf, len, NULL, true));

  // Now make a regular request.
  std::string headers;
  MockTransaction transaction(kRangeGET_TransactionOK);
  transaction.request_headers = EXTRA_HEADER;
  transaction.data = "rg: 00-09 rg: 10-19 rg: 20-29 rg: 30-39 rg: 40-49 "
                     "rg: 50-59 rg: 60-69 rg: 70-79 ";
  RunTransactionTestWithResponse(cache.http_cache(), transaction, &headers);

  // We update the headers with the ones received while revalidating.
  std::string expected_headers(
      "HTTP/1.1 200 OK\n"
      "Last-Modified: Sat, 18 Apr 2009 01:10:43 GMT\n"
      "Accept-Ranges: bytes\n"
      "ETag: \"foo\"\n"
      "Content-Length: 10\n");

  EXPECT_EQ(expected_headers, headers);
  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);

  // Verify that the disk entry was updated.
  EXPECT_EQ(80, entry->GetDataSize(1));
  bool truncated = true;
  EXPECT_TRUE(net::HttpCache::ReadResponseInfo(entry, &response, &truncated));
  EXPECT_FALSE(truncated);
  entry->Close();
}

// Tests that we can handle range requests when we have a truncated entry.
TEST(HttpCache, RangeGET_IncompleteResource) {
  MockHttpCache cache;
  cache.http_cache()->set_enable_range_support(true);
  AddMockTransaction(&kRangeGET_TransactionOK);

  // Create a disk cache entry that stores an incomplete resource.
  disk_cache::Entry* entry;
  ASSERT_TRUE(cache.disk_cache()->CreateEntry(kRangeGET_TransactionOK.url,
                                              &entry));

  // Content-length will be intentionally bogus.
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Last-Modified: something\n"
                          "ETag: \"foo\"\n"
                          "Accept-Ranges: bytes\n"
                          "Content-Length: 10\n");
  raw_headers = net::HttpUtil::AssembleRawHeaders(raw_headers.data(),
                                                  raw_headers.size());

  net::HttpResponseInfo response;
  response.headers = new net::HttpResponseHeaders(raw_headers);
  // Set the last argument for this to be an incomplete request.
  EXPECT_TRUE(net::HttpCache::WriteResponseInfo(entry, &response, true, true));

  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(100));
  int len = static_cast<int>(base::strlcpy(buf->data(),
                                           "rg: 00-09 rg: 10-19 ", 100));
  EXPECT_EQ(len, entry->WriteData(1, 0, buf, len, NULL, true));
  entry->Close();

  // Now make a range request.
  std::string headers;
  RunTransactionTestWithResponse(cache.http_cache(), kRangeGET_TransactionOK,
                                 &headers);

  EXPECT_TRUE(Verify206Response(headers, 40, 49));
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  RemoveMockTransaction(&kRangeGET_TransactionOK);
}

TEST(HttpCache, SyncRead) {
  MockHttpCache cache;

  // This test ensures that a read that completes synchronously does not cause
  // any problems.

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.test_mode |= (TEST_MODE_SYNC_CACHE_START |
                            TEST_MODE_SYNC_CACHE_READ |
                            TEST_MODE_SYNC_CACHE_WRITE);

  MockHttpRequest r1(transaction),
                  r2(transaction),
                  r3(transaction);

  TestTransactionConsumer c1(cache.http_cache()),
                          c2(cache.http_cache()),
                          c3(cache.http_cache());

  c1.Start(&r1, NULL);

  r2.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c2.Start(&r2, NULL);

  r3.load_flags |= net::LOAD_ONLY_FROM_CACHE;
  c3.Start(&r3, NULL);

  MessageLoop::current()->Run();

  EXPECT_TRUE(c1.is_done());
  EXPECT_TRUE(c2.is_done());
  EXPECT_TRUE(c3.is_done());

  EXPECT_EQ(net::OK, c1.error());
  EXPECT_EQ(net::OK, c2.error());
  EXPECT_EQ(net::OK, c3.error());
}

TEST(HttpCache, ValidationResultsIn200) {
  MockHttpCache cache;

  // This test ensures that a conditional request, which results in a 200
  // instead of a 304, properly truncates the existing response data.

  // write to the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);

  // force this transaction to validate the cache
  MockTransaction transaction(kETagGET_Transaction);
  transaction.load_flags |= net::LOAD_VALIDATE_CACHE;
  RunTransactionTest(cache.http_cache(), transaction);

  // read from the cache
  RunTransactionTest(cache.http_cache(), kETagGET_Transaction);
}

TEST(HttpCache, CachedRedirect) {
  MockHttpCache cache;

  ScopedMockTransaction kTestTransaction(kSimpleGET_Transaction);
  kTestTransaction.status = "HTTP/1.1 301 Moved Permanently";
  kTestTransaction.response_headers = "Location: http://www.bar.com/\n";

  MockHttpRequest request(kTestTransaction);
  TestCompletionCallback callback;

  // write to the cache
  {
    scoped_ptr<net::HttpTransaction> trans;
    int rv = cache.http_cache()->CreateTransaction(&trans);
    EXPECT_EQ(net::OK, rv);
    ASSERT_TRUE(trans.get());

    rv = trans->Start(&request, &callback, NULL);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // read from the cache
  {
    scoped_ptr<net::HttpTransaction> trans;
    int rv = cache.http_cache()->CreateTransaction(&trans);
    EXPECT_EQ(net::OK, rv);
    ASSERT_TRUE(trans.get());

    rv = trans->Start(&request, &callback, NULL);
    if (rv == net::ERR_IO_PENDING)
      rv = callback.WaitForResult();
    ASSERT_EQ(net::OK, rv);

    const net::HttpResponseInfo* info = trans->GetResponseInfo();
    ASSERT_TRUE(info);

    EXPECT_EQ(info->headers->response_code(), 301);

    std::string location;
    info->headers->EnumerateHeader(NULL, "Location", &location);
    EXPECT_EQ(location, "http://www.bar.com/");

    // Destroy transaction when going out of scope. We have not actually
    // read the response body -- want to test that it is still getting cached.
  }
  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

TEST(HttpCache, CacheControlNoStore) {
  MockHttpCache cache;

  ScopedMockTransaction transaction(kSimpleGET_Transaction);
  transaction.response_headers = "cache-control: no-store\n";

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(2, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore2) {
  // this test is similar to the above test, except that the initial response
  // is cachable, but when it is validated, no-store is received causing the
  // cached document to be deleted.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

TEST(HttpCache, CacheControlNoStore3) {
  // this test is similar to the above test, except that the response is a 304
  // instead of a 200.  this should never happen in practice, but it seems like
  // a good thing to verify that we still destroy the cache entry.
  MockHttpCache cache;

  ScopedMockTransaction transaction(kETagGET_Transaction);

  // initial load
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(1, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  // try loading again; it should result in a network fetch
  transaction.load_flags = net::LOAD_VALIDATE_CACHE;
  transaction.response_headers = "cache-control: no-store\n";
  transaction.status = "HTTP/1.1 304 Not Modified";
  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(1, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());

  disk_cache::Entry* entry;
  bool exists = cache.disk_cache()->OpenEntry(transaction.url, &entry);
  EXPECT_FALSE(exists);
}

// Ensure that we don't cache requests served over bad HTTPS.
TEST(HttpCache, SimpleGET_SSLError) {
  MockHttpCache cache;

  MockTransaction transaction = kSimpleGET_Transaction;
  transaction.cert_status = net::CERT_STATUS_REVOKED;
  ScopedMockTransaction scoped_transaction(transaction);

  // write to the cache
  RunTransactionTest(cache.http_cache(), transaction);

  // Test that it was not cached.
  transaction.load_flags |= net::LOAD_ONLY_FROM_CACHE;

  MockHttpRequest request(transaction);
  TestCompletionCallback callback;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache.http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);
  ASSERT_TRUE(trans.get());

  rv = trans->Start(&request, &callback, NULL);
  if (rv == net::ERR_IO_PENDING)
    rv = callback.WaitForResult();
  ASSERT_EQ(net::ERR_CACHE_MISS, rv);
}

// Ensure that we don't crash by if left-behind transactions.
TEST(HttpCache, OutlivedTransactions) {
  MockHttpCache* cache = new MockHttpCache;

  scoped_ptr<net::HttpTransaction> trans;
  int rv = cache->http_cache()->CreateTransaction(&trans);
  EXPECT_EQ(net::OK, rv);

  delete cache;
  trans.reset();
}

// Test that the disabled mode works.
TEST(HttpCache, CacheDisabledMode) {
  MockHttpCache cache;

  // write to the cache
  RunTransactionTest(cache.http_cache(), kSimpleGET_Transaction);

  // go into disabled mode
  cache.http_cache()->set_mode(net::HttpCache::DISABLE);

  // force this transaction to write to the cache again
  MockTransaction transaction(kSimpleGET_Transaction);

  RunTransactionTest(cache.http_cache(), transaction);

  EXPECT_EQ(2, cache.network_layer()->transaction_count());
  EXPECT_EQ(0, cache.disk_cache()->open_count());
  EXPECT_EQ(1, cache.disk_cache()->create_count());
}

// Other tests check that the response headers of the cached response
// get updated on 304. Here we specifically check that the
// HttpResponseHeaders::request_time and HttpResponseHeaders::response_time
// fields also gets updated.
// http://crbug.com/20594.
TEST(HttpCache, UpdatesRequestResponseTimeOn304) {
  MockHttpCache cache;

  const char* kUrl = "http://foobar";
  const char* kData = "body";

  MockTransaction mock_network_response = { 0 };
  mock_network_response.url = kUrl;

  AddMockTransaction(&mock_network_response);

  // Request |kUrl|, causing |kNetResponse1| to be written to the cache.

  MockTransaction request = { 0 };
  request.url = kUrl;
  request.method = "GET";
  request.request_headers = "";
  request.data = kData;

  static const Response kNetResponse1 = {
    "HTTP/1.1 200 OK",
    "Date: Fri, 12 Jun 2009 21:46:42 GMT\n"
    "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
    kData
  };

  kNetResponse1.AssignTo(&mock_network_response);

  RunTransactionTest(cache.http_cache(), request);

  // Request |kUrl| again, this time validating the cache and getting
  // a 304 back.

  request.load_flags = net::LOAD_VALIDATE_CACHE;

  static const Response kNetResponse2 = {
    "HTTP/1.1 304 Not Modified",
    "Date: Wed, 22 Jul 2009 03:15:26 GMT\n",
    ""
  };

  kNetResponse2.AssignTo(&mock_network_response);

  base::Time request_time = base::Time() + base::TimeDelta::FromHours(1234);
  base::Time response_time = base::Time() + base::TimeDelta::FromHours(1235);

  mock_network_response.request_time = request_time;
  mock_network_response.response_time = response_time;

  net::HttpResponseInfo response;
  RunTransactionTestWithResponseInfo(cache.http_cache(), request, &response);

  // The request and response times should have been updated.
  EXPECT_EQ(request_time.ToInternalValue(),
            response.request_time.ToInternalValue());
  EXPECT_EQ(response_time.ToInternalValue(),
            response.response_time.ToInternalValue());

  std::string headers;
  response.headers->GetNormalizedHeaders(&headers);

  EXPECT_EQ("HTTP/1.1 200 OK\n"
            "Date: Wed, 22 Jul 2009 03:15:26 GMT\n"
            "Last-Modified: Wed, 06 Feb 2008 22:38:21 GMT\n",
            headers);

  RemoveMockTransaction(&mock_network_response);
}
