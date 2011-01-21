// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/view_cache_helper.h"

#include "base/pickle.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/url_request/url_request_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestURLRequestContext : public net::URLRequestContext {
 public:
  TestURLRequestContext();

  // Gets a pointer to the cache backend.
  disk_cache::Backend* GetBackend();

 private:
  net::HttpCache cache_;
};

TestURLRequestContext::TestURLRequestContext()
    : cache_(reinterpret_cast<net::HttpTransactionFactory*>(NULL), NULL,
             net::HttpCache::DefaultBackend::InMemory(0)) {
  http_transaction_factory_ = &cache_;
}

void WriteHeaders(disk_cache::Entry* entry, int flags, const std::string data) {
  if (data.empty())
    return;

  Pickle pickle;
  pickle.WriteInt(flags | 1);  // Version 1.
  pickle.WriteInt64(0);
  pickle.WriteInt64(0);
  pickle.WriteString(data);

  scoped_refptr<net::WrappedIOBuffer> buf(new net::WrappedIOBuffer(
      reinterpret_cast<const char*>(pickle.data())));
  int len = static_cast<int>(pickle.size());

  TestCompletionCallback cb;
  int rv = entry->WriteData(0, 0, buf, len, &cb, true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteData(disk_cache::Entry* entry, int index, const std::string data) {
  if (data.empty())
    return;

  int len = data.length();
  scoped_refptr<net::IOBuffer> buf(new net::IOBuffer(len));
  memcpy(buf->data(), data.data(), data.length());

  TestCompletionCallback cb;
  int rv = entry->WriteData(index, 0, buf, len, &cb, true);
  ASSERT_EQ(len, cb.GetResult(rv));
}

void WriteToEntry(disk_cache::Backend* cache, const std::string key,
                  const std::string data0, const std::string data1,
                  const std::string data2) {
  TestCompletionCallback cb;
  disk_cache::Entry* entry;
  int rv = cache->CreateEntry(key, &entry, &cb);
  rv = cb.GetResult(rv);
  if (rv != net::OK) {
    rv = cache->OpenEntry(key, &entry, &cb);
    ASSERT_EQ(net::OK, cb.GetResult(rv));
  }

  WriteHeaders(entry, 0, data0);
  WriteData(entry, 1, data1);
  WriteData(entry, 2, data2);

  entry->Close();
}

void FillCache(net::URLRequestContext* context) {
  TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv =
      context->http_transaction_factory()->GetCache()->GetBackend(&cache, &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  std::string empty;
  WriteToEntry(cache, "first", "some", empty, empty);
  WriteToEntry(cache, "second", "only hex_dumped", "same", "kind");
  WriteToEntry(cache, "third", empty, "another", "thing");
}

}  // namespace.

TEST(ViewCacheHelper, EmptyCache) {
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext());
  net::ViewCacheHelper helper;

  TestCompletionCallback cb;
  std::string prefix, data;
  int rv = helper.GetContentsHTML(context, prefix, &data, &cb);
  EXPECT_EQ(net::OK, cb.GetResult(rv));
  EXPECT_FALSE(data.empty());
}

TEST(ViewCacheHelper, ListContents) {
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext());
  net::ViewCacheHelper helper;

  FillCache(context);

  std::string prefix, data;
  TestCompletionCallback cb;
  int rv = helper.GetContentsHTML(context, prefix, &data, &cb);
  EXPECT_EQ(net::OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));
  EXPECT_NE(std::string::npos, data.find("first"));
  EXPECT_NE(std::string::npos, data.find("second"));
  EXPECT_NE(std::string::npos, data.find("third"));

  EXPECT_EQ(std::string::npos, data.find("some"));
  EXPECT_EQ(std::string::npos, data.find("same"));
  EXPECT_EQ(std::string::npos, data.find("thing"));
}

TEST(ViewCacheHelper, DumpEntry) {
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext());
  net::ViewCacheHelper helper;

  FillCache(context);

  std::string data;
  TestCompletionCallback cb;
  int rv = helper.GetEntryInfoHTML("second", context, &data, &cb);
  EXPECT_EQ(net::OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));

  EXPECT_NE(std::string::npos, data.find("hex_dumped"));
  EXPECT_NE(std::string::npos, data.find("same"));
  EXPECT_NE(std::string::npos, data.find("kind"));

  EXPECT_EQ(std::string::npos, data.find("first"));
  EXPECT_EQ(std::string::npos, data.find("third"));
  EXPECT_EQ(std::string::npos, data.find("some"));
  EXPECT_EQ(std::string::npos, data.find("another"));
}

// Makes sure the links are correct.
TEST(ViewCacheHelper, Prefix) {
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext());
  net::ViewCacheHelper helper;

  FillCache(context);

  std::string key, data;
  std::string prefix("prefix:");
  TestCompletionCallback cb;
  int rv = helper.GetContentsHTML(context, prefix, &data, &cb);
  EXPECT_EQ(net::OK, cb.GetResult(rv));

  EXPECT_EQ(0U, data.find("<html>"));
  EXPECT_NE(std::string::npos, data.find("</html>"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:first\">"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:second\">"));
  EXPECT_NE(std::string::npos, data.find("<a href=\"prefix:third\">"));
}

TEST(ViewCacheHelper, TruncatedFlag) {
  scoped_refptr<TestURLRequestContext> context(new TestURLRequestContext());
  net::ViewCacheHelper helper;

  TestCompletionCallback cb;
  disk_cache::Backend* cache;
  int rv =
      context->http_transaction_factory()->GetCache()->GetBackend(&cache, &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  std::string key("the key");
  disk_cache::Entry* entry;
  rv = cache->CreateEntry(key, &entry, &cb);
  ASSERT_EQ(net::OK, cb.GetResult(rv));

  // RESPONSE_INFO_TRUNCATED defined on response_info.cc
  int flags = 1 << 12;
  WriteHeaders(entry, flags, "something");
  entry->Close();

  std::string data;
  rv = helper.GetEntryInfoHTML(key, context, &data, &cb);
  EXPECT_EQ(net::OK, cb.GetResult(rv));

  EXPECT_NE(std::string::npos, data.find("RESPONSE_INFO_TRUNCATED"));
}
