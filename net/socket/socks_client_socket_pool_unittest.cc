// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

class SOCKSClientSocketPoolTest : public testing::Test {
 protected:
  class SOCKS5MockData {
   public:
    explicit SOCKS5MockData(IoMode mode) {
      writes_.reset(new MockWrite[3]);
      writes_[0] = MockWrite(mode, kSOCKS5GreetRequest,
                             kSOCKS5GreetRequestLength);
      writes_[1] = MockWrite(mode, kSOCKS5OkRequest, kSOCKS5OkRequestLength);
      writes_[2] = MockWrite(mode, 0);

      reads_.reset(new MockRead[3]);
      reads_[0] = MockRead(mode, kSOCKS5GreetResponse,
                           kSOCKS5GreetResponseLength);
      reads_[1] = MockRead(mode, kSOCKS5OkResponse, kSOCKS5OkResponseLength);
      reads_[2] = MockRead(mode, 0);

      data_.reset(new StaticSocketDataProvider(reads_.get(), 3,
                                               writes_.get(), 3));
    }

    SocketDataProvider* data_provider() { return data_.get(); }

   private:
    scoped_ptr<StaticSocketDataProvider> data_;
    scoped_array<MockWrite> writes_;
    scoped_array<MockRead> reads_;
  };

  SOCKSClientSocketPoolTest()
      : ignored_transport_socket_params_(new TransportSocketParams(
          HostPortPair("proxy", 80), MEDIUM, false, false,
          OnHostResolutionCallback())),
        transport_histograms_("MockTCP"),
        transport_socket_pool_(
            kMaxSockets, kMaxSocketsPerGroup,
            &transport_histograms_,
            &transport_client_socket_factory_),
        ignored_socket_params_(new SOCKSSocketParams(
            ignored_transport_socket_params_, true, HostPortPair("host", 80),
            MEDIUM)),
        socks_histograms_("SOCKSUnitTest"),
        pool_(kMaxSockets, kMaxSocketsPerGroup,
              &socks_histograms_,
              NULL,
              &transport_socket_pool_,
              NULL) {
  }

  virtual ~SOCKSClientSocketPoolTest() {}

  int StartRequest(const std::string& group_name, RequestPriority priority) {
    return test_base_.StartRequestUsingPool(
        &pool_, group_name, priority, ignored_socket_params_);
  }

  int GetOrderOfRequest(size_t index) const {
    return test_base_.GetOrderOfRequest(index);
  }

  ScopedVector<TestSocketRequest>* requests() { return test_base_.requests(); }

  scoped_refptr<TransportSocketParams> ignored_transport_socket_params_;
  ClientSocketPoolHistograms transport_histograms_;
  MockClientSocketFactory transport_client_socket_factory_;
  MockTransportClientSocketPool transport_socket_pool_;

  scoped_refptr<SOCKSSocketParams> ignored_socket_params_;
  ClientSocketPoolHistograms socks_histograms_;
  SOCKSClientSocketPool pool_;
  ClientSocketPoolTest test_base_;
};

TEST_F(SOCKSClientSocketPoolTest, Simple) {
  SOCKS5MockData data(SYNCHRONOUS);
  data.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(data.data_provider());

  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, CompletionCallback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, Async) {
  SOCKS5MockData data(ASYNC);
  transport_client_socket_factory_.AddSocketDataProvider(data.data_provider());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, callback.callback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, TransportConnectError) {
  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS,
                                           ERR_CONNECTION_REFUSED));
  transport_client_socket_factory_.AddSocketDataProvider(&socket_data);

  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, CompletionCallback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, AsyncTransportConnectError) {
  StaticSocketDataProvider socket_data;
  socket_data.set_connect_data(MockConnect(ASYNC, ERR_CONNECTION_REFUSED));
  transport_client_socket_factory_.AddSocketDataProvider(&socket_data);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, callback.callback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_PROXY_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, SOCKSConnectError) {
  MockRead failed_read[] = {
    MockRead(SYNCHRONOUS, 0),
  };
  StaticSocketDataProvider socket_data(
      failed_read, arraysize(failed_read), NULL, 0);
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(&socket_data);

  ClientSocketHandle handle;
  EXPECT_EQ(0, transport_socket_pool_.release_count());
  int rv = handle.Init("a", ignored_socket_params_, LOW, CompletionCallback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(ERR_SOCKS_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(1, transport_socket_pool_.release_count());
}

TEST_F(SOCKSClientSocketPoolTest, AsyncSOCKSConnectError) {
  MockRead failed_read[] = {
    MockRead(ASYNC, 0),
  };
  StaticSocketDataProvider socket_data(
        failed_read, arraysize(failed_read), NULL, 0);
  socket_data.set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(&socket_data);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(0, transport_socket_pool_.release_count());
  int rv = handle.Init("a", ignored_socket_params_, LOW, callback.callback(),
                       &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_SOCKS_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(1, transport_socket_pool_.release_count());
}

TEST_F(SOCKSClientSocketPoolTest, CancelDuringTransportConnect) {
  SOCKS5MockData data(SYNCHRONOUS);
  transport_client_socket_factory_.AddSocketDataProvider(data.data_provider());
  // We need two connections because the pool base lets one cancelled
  // connect job proceed for potential future use.
  SOCKS5MockData data2(SYNCHRONOUS);
  transport_client_socket_factory_.AddSocketDataProvider(data2.data_provider());

  EXPECT_EQ(0, transport_socket_pool_.cancel_count());
  int rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  pool_.CancelRequest("a", (*requests())[0]->handle());
  pool_.CancelRequest("a", (*requests())[1]->handle());
  // Requests in the connect phase don't actually get cancelled.
  EXPECT_EQ(0, transport_socket_pool_.cancel_count());

  // Now wait for the TCP sockets to connect.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound, GetOrderOfRequest(1));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound, GetOrderOfRequest(2));
  EXPECT_EQ(0, transport_socket_pool_.cancel_count());
  EXPECT_EQ(2, pool_.IdleSocketCount());

  (*requests())[0]->handle()->Reset();
  (*requests())[1]->handle()->Reset();
}

TEST_F(SOCKSClientSocketPoolTest, CancelDuringSOCKSConnect) {
  SOCKS5MockData data(ASYNC);
  data.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(data.data_provider());
  // We need two connections because the pool base lets one cancelled
  // connect job proceed for potential future use.
  SOCKS5MockData data2(ASYNC);
  data2.data_provider()->set_connect_data(MockConnect(SYNCHRONOUS, OK));
  transport_client_socket_factory_.AddSocketDataProvider(data2.data_provider());

  EXPECT_EQ(0, transport_socket_pool_.cancel_count());
  EXPECT_EQ(0, transport_socket_pool_.release_count());
  int rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  pool_.CancelRequest("a", (*requests())[0]->handle());
  pool_.CancelRequest("a", (*requests())[1]->handle());
  EXPECT_EQ(0, transport_socket_pool_.cancel_count());
  // Requests in the connect phase don't actually get cancelled.
  EXPECT_EQ(0, transport_socket_pool_.release_count());

  // Now wait for the async data to reach the SOCKS connect jobs.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound, GetOrderOfRequest(1));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound, GetOrderOfRequest(2));
  EXPECT_EQ(0, transport_socket_pool_.cancel_count());
  EXPECT_EQ(0, transport_socket_pool_.release_count());
  EXPECT_EQ(2, pool_.IdleSocketCount());

  (*requests())[0]->handle()->Reset();
  (*requests())[1]->handle()->Reset();
}

// It would be nice to also test the timeouts in SOCKSClientSocketPool.

}  // namespace

}  // namespace net
