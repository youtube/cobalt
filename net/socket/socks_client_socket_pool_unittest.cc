// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket_pool.h"

#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;

class MockTCPClientSocketPool : public TCPClientSocketPool {
 public:
  class MockConnectJob {
   public:
    MockConnectJob(ClientSocket* socket, ClientSocketHandle* handle,
                   CompletionCallback* callback)
        : socket_(socket),
          handle_(handle),
          user_callback_(callback),
          ALLOW_THIS_IN_INITIALIZER_LIST(
              connect_callback_(this, &MockConnectJob::OnConnect)) {}

    int Connect(const BoundNetLog& net_log) {
      int rv = socket_->Connect(&connect_callback_, net_log);
      if (rv == OK) {
        user_callback_ = NULL;
        OnConnect(OK);
      }
      return rv;
    }

    bool CancelHandle(const ClientSocketHandle* handle) {
      if (handle != handle_)
        return false;
      socket_.reset(NULL);
      handle_ = NULL;
      user_callback_ = NULL;
      return true;
    }

   private:
    void OnConnect(int rv) {
      if (!socket_.get())
        return;
      if (rv == OK)
        handle_->set_socket(socket_.get());

      socket_.release();
      handle_ = NULL;

      if (user_callback_) {
        CompletionCallback* callback = user_callback_;
        user_callback_ = NULL;
        callback->Run(rv);
      }
    }

    scoped_ptr<ClientSocket> socket_;
    ClientSocketHandle* handle_;
    CompletionCallback* user_callback_;
    CompletionCallbackImpl<MockConnectJob> connect_callback_;

    DISALLOW_COPY_AND_ASSIGN(MockConnectJob);
  };

  MockTCPClientSocketPool(int max_sockets, int max_sockets_per_group,
                          const std::string& name,
                          ClientSocketFactory* socket_factory,
                          NetworkChangeNotifier* network_change_notifier)
      : TCPClientSocketPool(max_sockets, max_sockets_per_group, name,
                            NULL, NULL, network_change_notifier),
        client_socket_factory_(socket_factory),
        release_count_(0),
        cancel_count_(0) {}

  int release_count() { return release_count_; };
  int cancel_count() { return cancel_count_; };

  // TCPClientSocketPool methods.
  virtual int RequestSocket(const std::string& group_name,
                            const void* socket_params,
                            RequestPriority priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback,
                            const BoundNetLog& net_log) {
    ClientSocket* socket = client_socket_factory_->CreateTCPClientSocket(
        AddressList());
    MockConnectJob* job = new MockConnectJob(socket, handle, callback);
    job_list_.push_back(job);
    return job->Connect(net_log);
  }

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle) {
    std::vector<MockConnectJob*>::iterator i;
    for (i = job_list_.begin(); i != job_list_.end(); ++i) {
      if ((*i)->CancelHandle(handle)) {
        cancel_count_++;
        break;
      }
    }
  }

  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket) {
    release_count_++;
    delete socket;
  }

 protected:
  virtual ~MockTCPClientSocketPool() {}

 private:
  ClientSocketFactory* client_socket_factory_;
  int release_count_;
  int cancel_count_;
  std::vector<MockConnectJob*> job_list_;

  DISALLOW_COPY_AND_ASSIGN(MockTCPClientSocketPool);
};

class SOCKSClientSocketPoolTest : public ClientSocketPoolTest {
 protected:
  class SOCKS5MockData {
   public:
    explicit SOCKS5MockData(bool async) {
      writes_.reset(new MockWrite[3]);
      writes_[0] = MockWrite(async, kSOCKS5GreetRequest,
                             kSOCKS5GreetRequestLength);
      writes_[1] = MockWrite(async, kSOCKS5OkRequest, kSOCKS5OkRequestLength);
      writes_[2] = MockWrite(async, 0);

      reads_.reset(new MockRead[3]);
      reads_[0] = MockRead(async, kSOCKS5GreetResponse,
                           kSOCKS5GreetResponseLength);
      reads_[1] = MockRead(async, kSOCKS5OkResponse, kSOCKS5OkResponseLength);
      reads_[2] = MockRead(async, 0);

      data_.reset(new StaticSocketDataProvider(reads_.get(), 3,
                                               writes_.get(), 3));
    }

    SocketDataProvider* data_provider() { return data_.get(); }

   private:
    scoped_ptr<StaticSocketDataProvider> data_;
    scoped_array<MockWrite> writes_;
    scoped_array<MockWrite> reads_;
  };

  SOCKSClientSocketPoolTest()
      : ignored_tcp_socket_params_("proxy", 80, MEDIUM, GURL(), false),
        tcp_socket_pool_(new MockTCPClientSocketPool(
            kMaxSockets, kMaxSocketsPerGroup, "MockTCP",
            &tcp_client_socket_factory_, &tcp_notifier_)),
        ignored_socket_params_(ignored_tcp_socket_params_, true, "host", 80,
            MEDIUM, GURL()),
        pool_(new SOCKSClientSocketPool(kMaxSockets, kMaxSocketsPerGroup,
            "SOCKSUnitTest", NULL, tcp_socket_pool_.get(), &socks_notifier_)) {
  }

  int StartRequest(const std::string& group_name, RequestPriority priority) {
    return StartRequestUsingPool(
        pool_, group_name, priority, ignored_socket_params_);
  }

  TCPSocketParams ignored_tcp_socket_params_;
  MockClientSocketFactory tcp_client_socket_factory_;
  MockNetworkChangeNotifier tcp_notifier_;
  scoped_refptr<MockTCPClientSocketPool> tcp_socket_pool_;

  SOCKSSocketParams ignored_socket_params_;
  MockNetworkChangeNotifier socks_notifier_;
  scoped_refptr<SOCKSClientSocketPool> pool_;
};

TEST_F(SOCKSClientSocketPoolTest, Simple) {
  SOCKS5MockData data(false);
  data.data_provider()->set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(data.data_provider());

  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, NULL, pool_, NULL);
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, Async) {
  SOCKS5MockData data(true);
  tcp_client_socket_factory_.AddSocketDataProvider(data.data_provider());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, &callback, pool_,
                       NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, TCPConnectError) {
  SocketDataProvider* socket_data = new StaticSocketDataProvider();
  socket_data->set_connect_data(MockConnect(false, ERR_CONNECTION_REFUSED));
  tcp_client_socket_factory_.AddSocketDataProvider(socket_data);

  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, NULL, pool_, NULL);
  EXPECT_EQ(ERR_CONNECTION_REFUSED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, AsyncTCPConnectError) {
  SocketDataProvider* socket_data = new StaticSocketDataProvider();
  socket_data->set_connect_data(MockConnect(true, ERR_CONNECTION_REFUSED));
  tcp_client_socket_factory_.AddSocketDataProvider(socket_data);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", ignored_socket_params_, LOW, &callback, pool_,
                       NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_CONNECTION_REFUSED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
}

TEST_F(SOCKSClientSocketPoolTest, SOCKSConnectError) {
  MockRead failed_read[] = {
    MockRead(false, 0),
  };
  SocketDataProvider* socket_data =
      new StaticSocketDataProvider(failed_read, arraysize(failed_read),
                                   NULL, 0);
  socket_data->set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(socket_data);

  ClientSocketHandle handle;
  EXPECT_EQ(0, tcp_socket_pool_->release_count());
  int rv = handle.Init("a", ignored_socket_params_, LOW, NULL, pool_, NULL);
  EXPECT_EQ(ERR_SOCKS_CONNECTION_FAILED, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(1, tcp_socket_pool_->release_count());
}

TEST_F(SOCKSClientSocketPoolTest, AsyncSOCKSConnectError) {
  MockRead failed_read[] = {
    MockRead(true, 0),
  };
  SocketDataProvider* socket_data =
      new StaticSocketDataProvider(failed_read, arraysize(failed_read),
                                   NULL, 0);
  socket_data->set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(socket_data);

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(0, tcp_socket_pool_->release_count());
  int rv = handle.Init("a", ignored_socket_params_, LOW, &callback, pool_,
                       NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(ERR_SOCKS_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(1, tcp_socket_pool_->release_count());
}

TEST_F(SOCKSClientSocketPoolTest, CancelDuringTCPConnect) {
  SOCKS5MockData data(false);
  tcp_client_socket_factory_.AddSocketDataProvider(data.data_provider());
  // We need two connections because the pool base lets one cancelled
  // connect job proceed for potential future use.
  SOCKS5MockData data2(false);
  tcp_client_socket_factory_.AddSocketDataProvider(data2.data_provider());

  EXPECT_EQ(0, tcp_socket_pool_->cancel_count());
  int rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  pool_->CancelRequest("a", requests_[0]->handle());
  pool_->CancelRequest("a", requests_[1]->handle());
  EXPECT_EQ(1, tcp_socket_pool_->cancel_count());

  // Now wait for the TCP sockets to connect.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(1));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(2));
  EXPECT_EQ(1, tcp_socket_pool_->cancel_count());
  EXPECT_EQ(1, pool_->IdleSocketCount());

  requests_[0]->handle()->Reset();
  requests_[1]->handle()->Reset();
}

TEST_F(SOCKSClientSocketPoolTest, CancelDuringSOCKSConnect) {
  SOCKS5MockData data(true);
  data.data_provider()->set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(data.data_provider());
  // We need two connections because the pool base lets one cancelled
  // connect job proceed for potential future use.
  SOCKS5MockData data2(true);
  data2.data_provider()->set_connect_data(MockConnect(false, 0));
  tcp_client_socket_factory_.AddSocketDataProvider(data2.data_provider());

  EXPECT_EQ(0, tcp_socket_pool_->cancel_count());
  EXPECT_EQ(0, tcp_socket_pool_->release_count());
  int rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = StartRequest("a", LOW);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  pool_->CancelRequest("a", requests_[0]->handle());
  pool_->CancelRequest("a", requests_[1]->handle());
  EXPECT_EQ(0, tcp_socket_pool_->cancel_count());
  EXPECT_EQ(1, tcp_socket_pool_->release_count());

  // Now wait for the async data to reach the SOCKS connect jobs.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(1));
  EXPECT_EQ(kRequestNotFound, GetOrderOfRequest(2));
  EXPECT_EQ(0, tcp_socket_pool_->cancel_count());
  EXPECT_EQ(1, tcp_socket_pool_->release_count());
  EXPECT_EQ(1, pool_->IdleSocketCount());

  requests_[0]->handle()->Reset();
  requests_[1]->handle()->Reset();
}

// It would be nice to also test the timeouts in SOCKSClientSocketPool.

}  // namespace

}  // namespace net
