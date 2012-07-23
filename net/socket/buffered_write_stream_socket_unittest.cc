// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/buffered_write_stream_socket.h"

#include "base/message_loop.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class BufferedWriteStreamSocketTest : public testing::Test {
 public:
  void Finish() {
    MessageLoop::current()->RunAllPending();
    EXPECT_TRUE(data_->at_read_eof());
    EXPECT_TRUE(data_->at_write_eof());
  }

  void Initialize(MockWrite* writes, size_t writes_count) {
    data_.reset(new DeterministicSocketData(NULL, 0, writes, writes_count));
    data_->set_connect_data(MockConnect(SYNCHRONOUS, 0));
    if (writes_count) {
      data_->StopAfter(writes_count);
    }
    DeterministicMockTCPClientSocket* wrapped_socket =
        new DeterministicMockTCPClientSocket(net_log_.net_log(), data_.get());
    data_->set_socket(wrapped_socket->AsWeakPtr());
    socket_.reset(new BufferedWriteStreamSocket(wrapped_socket));
    socket_->Connect(callback_.callback());
  }

  void TestWrite(const char* text) {
    scoped_refptr<StringIOBuffer> buf(new StringIOBuffer(text));
    EXPECT_EQ(buf->size(),
              socket_->Write(buf.get(), buf->size(), callback_.callback()));
  }

  scoped_ptr<BufferedWriteStreamSocket> socket_;
  scoped_ptr<DeterministicSocketData> data_;
  BoundNetLog net_log_;
  TestCompletionCallback callback_;
};

TEST_F(BufferedWriteStreamSocketTest, SingleWrite) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "abc"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abc");
  Finish();
}

TEST_F(BufferedWriteStreamSocketTest, AsyncWrite) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "abc"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abc");
  data_->Run();
  Finish();
}

TEST_F(BufferedWriteStreamSocketTest, TwoWritesIntoOne) {
  MockWrite writes[] = {
    MockWrite(SYNCHRONOUS, 0, "abcdef"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abc");
  TestWrite("def");
  Finish();
}

TEST_F(BufferedWriteStreamSocketTest, WriteWhileBlocked) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "abc"),
    MockWrite(ASYNC, 1, "def"),
    MockWrite(ASYNC, 2, "ghi"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abc");
  MessageLoop::current()->RunAllPending();
  TestWrite("def");
  data_->RunFor(1);
  TestWrite("ghi");
  data_->RunFor(1);
  Finish();
}

TEST_F(BufferedWriteStreamSocketTest, ContinuesPartialWrite) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "abc"),
    MockWrite(ASYNC, 1, "def"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abcdef");
  data_->Run();
  Finish();
}

TEST_F(BufferedWriteStreamSocketTest, TwoSeparateWrites) {
  MockWrite writes[] = {
    MockWrite(ASYNC, 0, "abc"),
    MockWrite(ASYNC, 1, "def"),
  };
  Initialize(writes, arraysize(writes));
  TestWrite("abc");
  data_->RunFor(1);
  TestWrite("def");
  data_->RunFor(1);
  Finish();
}

}  // anonymous namespace

}  // namespace net
