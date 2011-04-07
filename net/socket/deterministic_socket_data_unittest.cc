// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

#include "testing/platform_test.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------

namespace {

static const char kMsg1[] = "\0hello!\xff";
static const int kLen1 = arraysize(kMsg1);
static const char kMsg2[] = "\012345678\0";
static const int kLen2 = arraysize(kMsg2);
static const char kMsg3[] = "bye!";
static const int kLen3 = arraysize(kMsg3);

}  // anonymous namespace

namespace net {

class DeterministicSocketDataTest : public PlatformTest {
 public:
  DeterministicSocketDataTest();

  virtual void TearDown();

 protected:
  void Initialize(MockRead* reads, size_t reads_count, MockWrite* writes,
                  size_t writes_count);

  void AssertSyncReadEquals(const char* data, int len);
  void AssertAsyncReadEquals(const char* data, int len);
  void AssertReadReturns(const char* data, int len, int rv);
  void AssertReadBufferEquals(const char* data, int len);

  void AssertSyncWriteEquals(const char* data, int len);
  void AssertAsyncWriteEquals(const char* data, int len);
  void AssertWriteReturns(const char* data, int len, int rv);

  TestCompletionCallback read_callback_;
  TestCompletionCallback write_callback_;
  ClientSocket* sock_;
  scoped_refptr<DeterministicSocketData> data_;

 private:
  scoped_refptr<IOBuffer> read_buf_;
  MockConnect connect_data_;

  GURL url_;
  HostPortPair endpoint_;
  scoped_refptr<TransportSocketParams> tcp_params_;
  ClientSocketPoolHistograms histograms_;
  DeterministicMockClientSocketFactory socket_factory_;
  MockTransportClientSocketPool socket_pool_;
  ClientSocketHandle connection_;

  DISALLOW_COPY_AND_ASSIGN(DeterministicSocketDataTest);
};

DeterministicSocketDataTest::DeterministicSocketDataTest()
    : read_callback_(),
      write_callback_(),
      sock_(NULL),
      data_(NULL),
      read_buf_(NULL),
      connect_data_(false, OK),
      url_("https://www.google.com"),
      endpoint_("www.google.com", 443),
      tcp_params_(new TransportSocketParams(endpoint_,
                                            LOWEST,
                                            url_,
                                            false,
                                            false)),
      histograms_(""),
      socket_pool_(10, 10, &histograms_, &socket_factory_) {
}

void DeterministicSocketDataTest::TearDown() {
  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
  PlatformTest::TearDown();
}

void DeterministicSocketDataTest::Initialize(MockRead* reads,
                                           size_t reads_count,
                                           MockWrite* writes,
                                           size_t writes_count) {
  data_ = new DeterministicSocketData(reads, reads_count, writes, writes_count);
  data_->set_connect_data(connect_data_);
  socket_factory_.AddSocketDataProvider(data_.get());

  // Perform the TCP connect
  EXPECT_EQ(OK,
            connection_.Init(endpoint_.ToString(),
                tcp_params_,
                LOWEST,
                NULL,
                reinterpret_cast<TransportClientSocketPool*>(&socket_pool_),
                BoundNetLog()));
  sock_ = connection_.socket();
}

void DeterministicSocketDataTest::AssertSyncReadEquals(const char* data,
                                                       int len) {
  // Issue the read, which will complete immediately
  AssertReadReturns(data, len, len);
  AssertReadBufferEquals(data, len);
}

void DeterministicSocketDataTest::AssertAsyncReadEquals(const char* data,
                                                        int len) {
  // Issue the read, which will be completed asynchronously
  AssertReadReturns(data, len, ERR_IO_PENDING);

  EXPECT_FALSE(read_callback_.have_result());
  EXPECT_TRUE(sock_->IsConnected());
  data_->RunFor(1);  // Runs 1 step, to cause the callbacks to be invoked

  // Now the read should complete
  ASSERT_EQ(len, read_callback_.WaitForResult());
  AssertReadBufferEquals(data, len);
}

void DeterministicSocketDataTest::AssertReadReturns(const char* data,
                                                    int len, int rv) {
  read_buf_ = new IOBuffer(len);
  ASSERT_EQ(rv, sock_->Read(read_buf_, len, &read_callback_));
}

void DeterministicSocketDataTest::AssertReadBufferEquals(const char* data,
                                                         int len) {
  ASSERT_EQ(std::string(data, len), std::string(read_buf_->data(), len));
}

void DeterministicSocketDataTest::AssertSyncWriteEquals(const char* data,
                                                         int len) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  memcpy(buf->data(), data, len);

  // Issue the write, which will complete immediately
  ASSERT_EQ(len, sock_->Write(buf, len, &write_callback_));
}

void DeterministicSocketDataTest::AssertAsyncWriteEquals(const char* data,
                                                         int len) {
  // Issue the read, which will be completed asynchronously
  AssertWriteReturns(data, len, ERR_IO_PENDING);

  EXPECT_FALSE(read_callback_.have_result());
  EXPECT_TRUE(sock_->IsConnected());
  data_->RunFor(1);  // Runs 1 step, to cause the callbacks to be invoked

  ASSERT_EQ(len, write_callback_.WaitForResult());
}

void DeterministicSocketDataTest::AssertWriteReturns(const char* data,
                                                     int len, int rv) {
  scoped_refptr<IOBuffer> buf(new IOBuffer(len));
  memcpy(buf->data(), data, len);

  // Issue the read, which will complete asynchronously
  ASSERT_EQ(rv, sock_->Write(buf, len, &write_callback_));
}

// ----------- Read

TEST_F(DeterministicSocketDataTest, SingleSyncReadWhileStopped) {
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 0),  // Sync Read
    MockRead(false, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  data_->SetStopped(true);
  AssertReadReturns(kMsg1, kLen1, ERR_UNEXPECTED);
}

TEST_F(DeterministicSocketDataTest, SingleSyncReadTooEarly) {
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 1),  // Sync Read
    MockRead(false, 0, 2),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  data_->StopAfter(2);
  ASSERT_FALSE(data_->stopped());
  AssertReadReturns(kMsg1, kLen1, ERR_UNEXPECTED);
}

TEST_F(DeterministicSocketDataTest, SingleSyncRead) {
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 0),  // Sync Read
    MockRead(false, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);
  // Make sure we don't stop before we've read all the data
  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MultipleSyncReads) {
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 0),  // Sync Read
    MockRead(false, kMsg2, kLen2, 1),  // Sync Read
    MockRead(false, kMsg3, kLen3, 2),  // Sync Read
    MockRead(false, kMsg3, kLen3, 3),  // Sync Read
    MockRead(false, kMsg2, kLen2, 4),  // Sync Read
    MockRead(false, kMsg3, kLen3, 5),  // Sync Read
    MockRead(false, kMsg1, kLen1, 6),  // Sync Read
    MockRead(false, 0, 7),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  // Make sure we don't stop before we've read all the data
  data_->StopAfter(10);
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, SingleAsyncRead) {
  MockRead reads[] = {
    MockRead(true, kMsg1, kLen1, 0),  // Async Read
    MockRead(false, 0, 1),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  AssertAsyncReadEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MultipleAsyncReads) {
  MockRead reads[] = {
      MockRead(true, kMsg1, kLen1, 0),  // Async Read
      MockRead(true, kMsg2, kLen2, 1),  // Async Read
      MockRead(true, kMsg3, kLen3, 2),  // Async Read
      MockRead(true, kMsg3, kLen3, 3),  // Async Read
      MockRead(true, kMsg2, kLen2, 4),  // Async Read
      MockRead(true, kMsg3, kLen3, 5),  // Async Read
      MockRead(true, kMsg1, kLen1, 6),  // Async Read
      MockRead(false, 0, 7),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertAsyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MixedReads) {
  MockRead reads[] = {
      MockRead(false, kMsg1, kLen1, 0),  // Sync Read
      MockRead(true, kMsg2, kLen2, 1),   // Async Read
      MockRead(false, kMsg3, kLen3, 2),  // Sync Read
      MockRead(true, kMsg3, kLen3, 3),   // Async Read
      MockRead(false, kMsg2, kLen2, 4),  // Sync Read
      MockRead(true, kMsg3, kLen3, 5),   // Async Read
      MockRead(false, kMsg1, kLen1, 6),  // Sync Read
      MockRead(false, 0, 7),  // EOF
  };

  Initialize(reads, arraysize(reads), NULL, 0);

  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertAsyncReadEquals(kMsg2, kLen2);
  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg3, kLen3);
  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg2, kLen2);
  AssertAsyncReadEquals(kMsg3, kLen3);
  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg1, kLen1);
}

// ----------- Write

TEST_F(DeterministicSocketDataTest, SingleSyncWriteWhileStopped) {
  MockWrite writes[] = {
    MockWrite(false, kMsg1, kLen1, 0),  // Sync Read
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  data_->SetStopped(true);
  AssertWriteReturns(kMsg1, kLen1, ERR_UNEXPECTED);
}

TEST_F(DeterministicSocketDataTest, SingleSyncWriteTooEarly) {
  MockWrite writes[] = {
    MockWrite(false, kMsg1, kLen1, 1),  // Sync Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  data_->StopAfter(2);
  ASSERT_FALSE(data_->stopped());
  AssertWriteReturns(kMsg1, kLen1, ERR_UNEXPECTED);
}

TEST_F(DeterministicSocketDataTest, SingleSyncWrite) {
  MockWrite writes[] = {
    MockWrite(false, kMsg1, kLen1, 0),  // Sync Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  // Make sure we don't stop before we've read all the data
  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MultipleSyncWrites) {
  MockWrite writes[] = {
    MockWrite(false, kMsg1, kLen1, 0),  // Sync Write
    MockWrite(false, kMsg2, kLen2, 1),  // Sync Write
    MockWrite(false, kMsg3, kLen3, 2),  // Sync Write
    MockWrite(false, kMsg3, kLen3, 3),  // Sync Write
    MockWrite(false, kMsg2, kLen2, 4),  // Sync Write
    MockWrite(false, kMsg3, kLen3, 5),  // Sync Write
    MockWrite(false, kMsg1, kLen1, 6),  // Sync Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  // Make sure we don't stop before we've read all the data
  data_->StopAfter(10);
  AssertSyncWriteEquals(kMsg1, kLen1);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncWriteEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, SingleAsyncWrite) {
  MockWrite writes[] = {
    MockWrite(true, kMsg1, kLen1, 0),  // Async Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  AssertAsyncWriteEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MultipleAsyncWrites) {
  MockWrite writes[] = {
    MockWrite(true, kMsg1, kLen1, 0),  // Async Write
    MockWrite(true, kMsg2, kLen2, 1),  // Async Write
    MockWrite(true, kMsg3, kLen3, 2),  // Async Write
    MockWrite(true, kMsg3, kLen3, 3),  // Async Write
    MockWrite(true, kMsg2, kLen2, 4),  // Async Write
    MockWrite(true, kMsg3, kLen3, 5),  // Async Write
    MockWrite(true, kMsg1, kLen1, 6),  // Async Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  AssertAsyncWriteEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg1, kLen1);
}

TEST_F(DeterministicSocketDataTest, MixedWrites) {
  MockWrite writes[] = {
    MockWrite(false, kMsg1, kLen1, 0),  // Sync Write
    MockWrite(true, kMsg2, kLen2, 1),   // Async Write
    MockWrite(false, kMsg3, kLen3, 2),  // Sync Write
    MockWrite(true, kMsg3, kLen3, 3),   // Async Write
    MockWrite(false, kMsg2, kLen2, 4),  // Sync Write
    MockWrite(true, kMsg3, kLen3, 5),   // Async Write
    MockWrite(false, kMsg1, kLen1, 6),  // Sync Write
  };

  Initialize(NULL, 0, writes, arraysize(writes));

  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg1, kLen1);
}

// ----------- Mixed Reads and Writes

TEST_F(DeterministicSocketDataTest, MixedSyncOperations) {
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 0),  // Sync Read
    MockRead(false, kMsg2, kLen2, 3),  // Sync Read
    MockRead(false, 0, 4),  // EOF
  };

  MockWrite writes[] = {
    MockWrite(false, kMsg2, kLen2, 1),  // Sync Write
    MockWrite(false, kMsg3, kLen3, 2),  // Sync Write
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  // Make sure we don't stop before we've read/written everything
  data_->StopAfter(10);
  AssertSyncReadEquals(kMsg1, kLen1);
  AssertSyncWriteEquals(kMsg2, kLen2);
  AssertSyncWriteEquals(kMsg3, kLen3);
  AssertSyncReadEquals(kMsg2, kLen2);
}

TEST_F(DeterministicSocketDataTest, MixedAsyncOperations) {
  MockRead reads[] = {
    MockRead(true, kMsg1, kLen1, 0),  // Sync Read
    MockRead(true, kMsg2, kLen2, 3),  // Sync Read
    MockRead(true, 0, 4),  // EOF
  };

  MockWrite writes[] = {
    MockWrite(true, kMsg2, kLen2, 1),  // Sync Write
    MockWrite(true, kMsg3, kLen3, 2),  // Sync Write
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  AssertAsyncReadEquals(kMsg1, kLen1);
  AssertAsyncWriteEquals(kMsg2, kLen2);
  AssertAsyncWriteEquals(kMsg3, kLen3);
  AssertAsyncReadEquals(kMsg2, kLen2);
}

TEST_F(DeterministicSocketDataTest, InterleavedAsyncOperations) {
  // Order of completion is read, write, write, read
  MockRead reads[] = {
    MockRead(true, kMsg1, kLen1, 0),  // Async Read
    MockRead(true, kMsg2, kLen2, 3),  // Async Read
    MockRead(true, 0, 4),  // EOF
  };

  MockWrite writes[] = {
    MockWrite(true, kMsg2, kLen2, 1),  // Async Write
    MockWrite(true, kMsg3, kLen3, 2),  // Async Write
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  // Issue the write, which will block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the read which will return first
  AssertReadReturns(kMsg1, kLen1, ERR_IO_PENDING);

  data_->RunFor(1);
  ASSERT_EQ(kLen1, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg1, kLen1);

  data_->RunFor(1);
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  data_->StopAfter(1);
  // Issue the read, which will block until the write completes
  AssertReadReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the writes which will return first
  AssertWriteReturns(kMsg3, kLen3, ERR_IO_PENDING);

  data_->RunFor(1);
  ASSERT_EQ(kLen3, write_callback_.WaitForResult());

  data_->RunFor(1);
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);
}

TEST_F(DeterministicSocketDataTest, InterleavedMixedOperations) {
  // Order of completion is read, write, write, read
  MockRead reads[] = {
    MockRead(false, kMsg1, kLen1, 0),  // Sync Read
    MockRead(true, kMsg2, kLen2, 3),   // Async Read
    MockRead(false, 0, 4),  // EOF
  };

  MockWrite writes[] = {
    MockWrite(true, kMsg2, kLen2, 1),   // Async Write
    MockWrite(false, kMsg3, kLen3, 2),  // Sync Write
  };

  Initialize(reads, arraysize(reads), writes, arraysize(writes));

  // Issue the write, which will block until the read completes
  AssertWriteReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately
  data_->StopAfter(1);
  AssertSyncReadEquals(kMsg1, kLen1);

  data_->RunFor(1);
  ASSERT_EQ(kLen2, write_callback_.WaitForResult());

  // Issue the read, which will block until the write completes
  AssertReadReturns(kMsg2, kLen2, ERR_IO_PENDING);

  // Issue the writes which will complete immediately
  data_->StopAfter(1);
  AssertSyncWriteEquals(kMsg3, kLen3);

  data_->RunFor(1);
  ASSERT_EQ(kLen2, read_callback_.WaitForResult());
  AssertReadBufferEquals(kMsg2, kLen2);
}

}  // namespace net
