// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_client_socket_pool.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/threading/platform_thread.h"
#include "net/base/ip_endpoint.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool_histograms.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_host_info.h"
#include "net/socket/stream_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

using internal::ClientSocketPoolBaseHelper;

namespace {

const int kMaxSockets = 32;
const int kMaxSocketsPerGroup = 6;
const net::RequestPriority kDefaultPriority = LOW;

void SetIPv4Address(IPEndPoint* address) {
  IPAddressNumber number;
  CHECK(ParseIPLiteralToNumber("1.1.1.1", &number));
  *address = IPEndPoint(number, 80);
}

void SetIPv6Address(IPEndPoint* address) {
  IPAddressNumber number;
  CHECK(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  *address = IPEndPoint(number, 80);
}

class MockClientSocket : public StreamSocket {
 public:
  MockClientSocket(const AddressList& addrlist)
      : connected_(false),
        addrlist_(addrlist) {}

  // StreamSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    connected_ = true;
    return OK;
  }
  virtual void Disconnect() {
    connected_ = false;
  }
  virtual bool IsConnected() const {
    return connected_;
  }
  virtual bool IsConnectedAndIdle() const {
    return connected_;
  }
  virtual int GetPeerAddress(AddressList* address) const {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const {
    if (!connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.head()->ai_family == AF_INET)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() {}
  virtual void SetOmniboxSpeculation() {}
  virtual bool WasEverUsed() const { return false; }
  virtual bool UsingTCPFastOpen() const { return false; }
  virtual int64 NumBytesRead() const { return -1; }
  virtual base::TimeDelta GetConnectTimeMicros() const {
    return base::TimeDelta::FromMicroseconds(-1);
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }
  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }
  virtual bool SetReceiveBufferSize(int32 size) { return true; }
  virtual bool SetSendBufferSize(int32 size) { return true; }

 private:
  bool connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;
};

class MockFailingClientSocket : public StreamSocket {
 public:
  MockFailingClientSocket(const AddressList& addrlist) : addrlist_(addrlist) {}

  // StreamSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    return ERR_CONNECTION_FAILED;
  }

  virtual void Disconnect() {}

  virtual bool IsConnected() const {
    return false;
  }
  virtual bool IsConnectedAndIdle() const {
    return false;
  }
  virtual int GetPeerAddress(AddressList* address) const {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const {
    return ERR_UNEXPECTED;
  }
  virtual const BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() {}
  virtual void SetOmniboxSpeculation() {}
  virtual bool WasEverUsed() const { return false; }
  virtual bool UsingTCPFastOpen() const { return false; }
  virtual int64 NumBytesRead() const { return -1; }
  virtual base::TimeDelta GetConnectTimeMicros() const {
    return base::TimeDelta::FromMicroseconds(-1);
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }
  virtual bool SetReceiveBufferSize(int32 size) { return true; }
  virtual bool SetSendBufferSize(int32 size) { return true; }

 private:
  const AddressList addrlist_;
  BoundNetLog net_log_;
};

class MockPendingClientSocket : public StreamSocket {
 public:
  // |should_connect| indicates whether the socket should successfully complete
  // or fail.
  // |should_stall| indicates that this socket should never connect.
  // |delay_ms| is the delay, in milliseconds, before simulating a connect.
  MockPendingClientSocket(
      const AddressList& addrlist,
      bool should_connect,
      bool should_stall,
      int delay_ms)
      : method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        should_connect_(should_connect),
        should_stall_(should_stall),
        delay_ms_(delay_ms),
        is_connected_(false),
        addrlist_(addrlist) {}

  // StreamSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        method_factory_.NewRunnableMethod(
           &MockPendingClientSocket::DoCallback, callback), delay_ms_);
    return ERR_IO_PENDING;
  }

  virtual void Disconnect() {}

  virtual bool IsConnected() const {
    return is_connected_;
  }
  virtual bool IsConnectedAndIdle() const {
    return is_connected_;
  }
  virtual int GetPeerAddress(AddressList* address) const {
    return ERR_UNEXPECTED;
  }
  virtual int GetLocalAddress(IPEndPoint* address) const {
    if (!is_connected_)
      return ERR_SOCKET_NOT_CONNECTED;
    if (addrlist_.head()->ai_family == AF_INET)
      SetIPv4Address(address);
    else
      SetIPv6Address(address);
    return OK;
  }
  virtual const BoundNetLog& NetLog() const {
    return net_log_;
  }

  virtual void SetSubresourceSpeculation() {}
  virtual void SetOmniboxSpeculation() {}
  virtual bool WasEverUsed() const { return false; }
  virtual bool UsingTCPFastOpen() const { return false; }
  virtual int64 NumBytesRead() const { return -1; }
  virtual base::TimeDelta GetConnectTimeMicros() const {
    return base::TimeDelta::FromMicroseconds(-1);
  }

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len,
                   CompletionCallback* callback) {
    return ERR_FAILED;
  }

  virtual int Write(IOBuffer* buf, int buf_len,
                    CompletionCallback* callback) {
    return ERR_FAILED;
  }
  virtual bool SetReceiveBufferSize(int32 size) { return true; }
  virtual bool SetSendBufferSize(int32 size) { return true; }

 private:
  void DoCallback(CompletionCallback* callback) {
    if (should_stall_)
      return;

    if (should_connect_) {
      is_connected_ = true;
      callback->Run(OK);
    } else {
      is_connected_ = false;
      callback->Run(ERR_CONNECTION_FAILED);
    }
  }

  ScopedRunnableMethodFactory<MockPendingClientSocket> method_factory_;
  bool should_connect_;
  bool should_stall_;
  int delay_ms_;
  bool is_connected_;
  const AddressList addrlist_;
  BoundNetLog net_log_;
};

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  enum ClientSocketType {
    MOCK_CLIENT_SOCKET,
    MOCK_FAILING_CLIENT_SOCKET,
    MOCK_PENDING_CLIENT_SOCKET,
    MOCK_PENDING_FAILING_CLIENT_SOCKET,
    // A delayed socket will pause before connecting through the message loop.
    MOCK_DELAYED_CLIENT_SOCKET,
    // A stalled socket that never connects at all.
    MOCK_STALLED_CLIENT_SOCKET,
  };

  MockClientSocketFactory()
      : allocation_count_(0), client_socket_type_(MOCK_CLIENT_SOCKET),
        client_socket_types_(NULL), client_socket_index_(0),
        client_socket_index_max_(0),
        delay_ms_(ClientSocketPool::kMaxConnectRetryIntervalMs) {}

  virtual DatagramClientSocket* CreateDatagramClientSocket(
      DatagramSocket::BindType bind_type,
      const RandIntCallback& rand_int_cb,
      NetLog* net_log,
      const NetLog::Source& source) {
    NOTREACHED();
    return NULL;
  }

  virtual StreamSocket* CreateTransportClientSocket(
      const AddressList& addresses,
      NetLog* /* net_log */,
      const NetLog::Source& /* source */) {
    allocation_count_++;

    ClientSocketType type = client_socket_type_;
    if (client_socket_types_ &&
        client_socket_index_ < client_socket_index_max_) {
      type = client_socket_types_[client_socket_index_++];
    }

    switch (type) {
      case MOCK_CLIENT_SOCKET:
        return new MockClientSocket(addresses);
      case MOCK_FAILING_CLIENT_SOCKET:
        return new MockFailingClientSocket(addresses);
      case MOCK_PENDING_CLIENT_SOCKET:
        return new MockPendingClientSocket(addresses, true, false, 0);
      case MOCK_PENDING_FAILING_CLIENT_SOCKET:
        return new MockPendingClientSocket(addresses, false, false, 0);
      case MOCK_DELAYED_CLIENT_SOCKET:
        return new MockPendingClientSocket(addresses, true, false, delay_ms_);
      case MOCK_STALLED_CLIENT_SOCKET:
        return new MockPendingClientSocket(addresses, true, true, 0);
      default:
        NOTREACHED();
        return new MockClientSocket(addresses);
    }
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocketHandle* transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config,
      SSLHostInfo* ssl_host_info,
      const SSLClientSocketContext& context) {
    NOTIMPLEMENTED();
    delete ssl_host_info;
    return NULL;
  }

  virtual void ClearSSLSessionCache() {
    NOTIMPLEMENTED();
  }

  int allocation_count() const { return allocation_count_; }

  // Set the default ClientSocketType.
  void set_client_socket_type(ClientSocketType type) {
    client_socket_type_ = type;
  }

  // Set a list of ClientSocketTypes to be used.
  void set_client_socket_types(ClientSocketType* type_list, int num_types) {
    DCHECK_GT(num_types, 0);
    client_socket_types_ = type_list;
    client_socket_index_ = 0;
    client_socket_index_max_ = num_types;
  }

  void set_delay_ms(int delay_ms) { delay_ms_ = delay_ms; }

 private:
  int allocation_count_;
  ClientSocketType client_socket_type_;
  ClientSocketType* client_socket_types_;
  int client_socket_index_;
  int client_socket_index_max_;
  int delay_ms_;
};

class TransportClientSocketPoolTest : public testing::Test {
 protected:
  TransportClientSocketPoolTest()
      : connect_backup_jobs_enabled_(
          ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(true)),
        params_(
            new TransportSocketParams(HostPortPair("www.google.com", 80),
                                     kDefaultPriority, GURL(), false, false)),
        low_params_(
            new TransportSocketParams(HostPortPair("www.google.com", 80),
                                      LOW, GURL(), false, false)),
        histograms_(new ClientSocketPoolHistograms("TCPUnitTest")),
        host_resolver_(new MockHostResolver),
        pool_(kMaxSockets,
              kMaxSocketsPerGroup,
              histograms_.get(),
              host_resolver_.get(),
              &client_socket_factory_,
              NULL) {
  }

  ~TransportClientSocketPoolTest() {
    internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        connect_backup_jobs_enabled_);
  }

  int StartRequest(const std::string& group_name, RequestPriority priority) {
    scoped_refptr<TransportSocketParams> params(new TransportSocketParams(
        HostPortPair("www.google.com", 80), MEDIUM, GURL(), false, false));
    return test_base_.StartRequestUsingPool(
        &pool_, group_name, priority, params);
  }

  int GetOrderOfRequest(size_t index) {
    return test_base_.GetOrderOfRequest(index);
  }

  bool ReleaseOneConnection(ClientSocketPoolTest::KeepAlive keep_alive) {
    return test_base_.ReleaseOneConnection(keep_alive);
  }

  void ReleaseAllConnections(ClientSocketPoolTest::KeepAlive keep_alive) {
    test_base_.ReleaseAllConnections(keep_alive);
  }

  ScopedVector<TestSocketRequest>* requests() { return test_base_.requests(); }
  size_t completion_count() const { return test_base_.completion_count(); }

  bool connect_backup_jobs_enabled_;
  scoped_refptr<TransportSocketParams> params_;
  scoped_refptr<TransportSocketParams> low_params_;
  scoped_ptr<ClientSocketPoolHistograms> histograms_;
  scoped_ptr<MockHostResolver> host_resolver_;
  MockClientSocketFactory client_socket_factory_;
  TransportClientSocketPool pool_;
  ClientSocketPoolTest test_base_;
};

TEST(TransportConnectJobTest, MakeAddrListStartWithIPv4) {
  IPAddressNumber ip_number;
  ASSERT_TRUE(ParseIPLiteralToNumber("192.168.1.1", &ip_number));
  AddressList addrlist_v4_1 = AddressList::CreateFromIPAddress(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("192.168.1.2", &ip_number));
  AddressList addrlist_v4_2 = AddressList::CreateFromIPAddress(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("2001:4860:b006::64", &ip_number));
  AddressList addrlist_v6_1 = AddressList::CreateFromIPAddress(ip_number, 80);
  ASSERT_TRUE(ParseIPLiteralToNumber("2001:4860:b006::66", &ip_number));
  AddressList addrlist_v6_2 = AddressList::CreateFromIPAddress(ip_number, 80);

  AddressList addrlist;
  const struct addrinfo* ai;

  // Test 1: IPv4 only.  Expect no change.
  addrlist = addrlist_v4_1;
  addrlist.Append(addrlist_v4_2.head());
  TransportConnectJob::MakeAddrListStartWithIPv4(&addrlist);
  ai = addrlist.head();
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET, ai->ai_family);
  EXPECT_TRUE(ai->ai_next == NULL);

  // Test 2: IPv6 only.  Expect no change.
  addrlist = addrlist_v6_1;
  addrlist.Append(addrlist_v6_2.head());
  TransportConnectJob::MakeAddrListStartWithIPv4(&addrlist);
  ai = addrlist.head();
  EXPECT_EQ(AF_INET6, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  EXPECT_TRUE(ai->ai_next == NULL);

  // Test 3: IPv4 then IPv6.  Expect no change.
  addrlist = addrlist_v4_1;
  addrlist.Append(addrlist_v4_2.head());
  addrlist.Append(addrlist_v6_1.head());
  addrlist.Append(addrlist_v6_2.head());
  TransportConnectJob::MakeAddrListStartWithIPv4(&addrlist);
  ai = addrlist.head();
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  EXPECT_TRUE(ai->ai_next == NULL);

  // Test 4: IPv6, IPv4, IPv6, IPv4.  Expect first IPv6 moved to the end.
  addrlist = addrlist_v6_1;
  addrlist.Append(addrlist_v4_1.head());
  addrlist.Append(addrlist_v6_2.head());
  addrlist.Append(addrlist_v4_2.head());
  TransportConnectJob::MakeAddrListStartWithIPv4(&addrlist);
  ai = addrlist.head();
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  EXPECT_TRUE(ai->ai_next == NULL);

  // Test 5: IPv6, IPv6, IPv4, IPv4.  Expect first two IPv6's moved to the end.
  addrlist = addrlist_v6_1;
  addrlist.Append(addrlist_v6_2.head());
  addrlist.Append(addrlist_v4_1.head());
  addrlist.Append(addrlist_v4_2.head());
  TransportConnectJob::MakeAddrListStartWithIPv4(&addrlist);
  ai = addrlist.head();
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  ai = ai->ai_next;
  EXPECT_EQ(AF_INET6, ai->ai_family);
  EXPECT_TRUE(ai->ai_next == NULL);
}

TEST_F(TransportClientSocketPoolTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, InitHostResolutionFailure) {
  host_resolver_->rules()->AddSimulatedFailure("unresolvable.host.name");
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  HostPortPair host_port_pair("unresolvable.host.name", 80);
  scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
          host_port_pair, kDefaultPriority, GURL(), false, false));
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", dest, kDefaultPriority, &callback, &pool_,
                        BoundNetLog()));
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, callback.WaitForResult());
}

TEST_F(TransportClientSocketPoolTest, InitConnectionFailure) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET);
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", params_, kDefaultPriority,
                                        &callback, &pool_, BoundNetLog()));
  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());

  // Make the host resolutions complete synchronously this time.
  host_resolver_->set_synchronous_mode(true);
  EXPECT_EQ(ERR_CONNECTION_FAILED, handle.Init("a", params_,
                                               kDefaultPriority, &callback,
                                               &pool_, BoundNetLog()));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // The rest are pending since we've used all active sockets.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());

  // One initial asynchronous request and then 10 pending requests.
  EXPECT_EQ(11U, completion_count());

  // First part of requests, all with the same priority, finishes in FIFO order.
  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));

  // Make sure that rest of the requests complete in the order of priority.
  EXPECT_EQ(7, GetOrderOfRequest(7));
  EXPECT_EQ(14, GetOrderOfRequest(8));
  EXPECT_EQ(15, GetOrderOfRequest(9));
  EXPECT_EQ(10, GetOrderOfRequest(10));
  EXPECT_EQ(13, GetOrderOfRequest(11));
  EXPECT_EQ(8, GetOrderOfRequest(12));
  EXPECT_EQ(16, GetOrderOfRequest(13));
  EXPECT_EQ(11, GetOrderOfRequest(14));
  EXPECT_EQ(12, GetOrderOfRequest(15));
  EXPECT_EQ(9, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

TEST_F(TransportClientSocketPoolTest, PendingRequests_NoKeepAlive) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  // Rest of them finish synchronously, until we reach the per-group limit.
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // The rest are pending since we've used all active sockets.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  ReleaseAllConnections(ClientSocketPoolTest::NO_KEEP_ALIVE);

  // The pending requests should finish successfully.
  EXPECT_EQ(OK, (*requests())[6]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[7]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[8]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[9]->WaitForResult());
  EXPECT_EQ(OK, (*requests())[10]->WaitForResult());

  EXPECT_EQ(static_cast<int>(requests()->size()),
            client_socket_factory_.allocation_count());

  // First asynchronous request, and then last 5 pending requests.
  EXPECT_EQ(6U, completion_count());
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending host resolution will eventually complete, and destroy the
// ClientSocketPool which will crash if the group was not cleared properly.
TEST_F(TransportClientSocketPoolTest, CancelRequestClearGroup) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", params_, kDefaultPriority,
                                        &callback, &pool_, BoundNetLog()));
  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, TwoRequestsCancelOne) {
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  ClientSocketHandle handle2;
  TestCompletionCallback callback2;

  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", params_, kDefaultPriority,
                                        &callback, &pool_, BoundNetLog()));
  EXPECT_EQ(ERR_IO_PENDING, handle2.Init("a", params_, kDefaultPriority,
                                         &callback2, &pool_, BoundNetLog()));

  handle.Reset();

  EXPECT_EQ(OK, callback2.WaitForResult());
  handle2.Reset();
}

TEST_F(TransportClientSocketPoolTest, ConnectCancelConnect) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);
  ClientSocketHandle handle;
  TestCompletionCallback callback;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", params_, kDefaultPriority,
                                        &callback, &pool_, BoundNetLog()));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING, handle.Init("a", params_, kDefaultPriority,
                                        &callback2, &pool_, BoundNetLog()));

  host_resolver_->set_synchronous_mode(true);
  // At this point, handle has two ConnectingSockets out for it.  Due to the
  // setting the mock resolver into synchronous mode, the host resolution for
  // both will return in the same loop of the MessageLoop.  The client socket
  // is a pending socket, so the Connect() will asynchronously complete on the
  // next loop of the MessageLoop.  That means that the first
  // ConnectingSocket will enter OnIOComplete, and then the second one will.
  // If the first one is not cancelled, it will advance the load state, and
  // then the second one will crash.

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(TransportClientSocketPoolTest, CancelRequest) {
  // First request finishes asynchronously.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, (*requests())[0]->WaitForResult());

  // Make all subsequent host resolutions complete synchronously.
  host_resolver_->set_synchronous_mode(true);

  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(OK, StartRequest("a", kDefaultPriority));

  // Reached per-group limit, queue up requests.
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", MEDIUM));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", HIGHEST));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOW));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", LOWEST));

  // Cancel a request.
  size_t index_to_cancel = kMaxSocketsPerGroup + 2;
  EXPECT_FALSE((*requests())[index_to_cancel]->handle()->is_initialized());
  (*requests())[index_to_cancel]->handle()->Reset();

  ReleaseAllConnections(ClientSocketPoolTest::KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup,
            client_socket_factory_.allocation_count());
  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());

  EXPECT_EQ(1, GetOrderOfRequest(1));
  EXPECT_EQ(2, GetOrderOfRequest(2));
  EXPECT_EQ(3, GetOrderOfRequest(3));
  EXPECT_EQ(4, GetOrderOfRequest(4));
  EXPECT_EQ(5, GetOrderOfRequest(5));
  EXPECT_EQ(6, GetOrderOfRequest(6));
  EXPECT_EQ(14, GetOrderOfRequest(7));
  EXPECT_EQ(7, GetOrderOfRequest(8));
  EXPECT_EQ(ClientSocketPoolTest::kRequestNotFound,
            GetOrderOfRequest(9));  // Canceled request.
  EXPECT_EQ(9, GetOrderOfRequest(10));
  EXPECT_EQ(10, GetOrderOfRequest(11));
  EXPECT_EQ(11, GetOrderOfRequest(12));
  EXPECT_EQ(8, GetOrderOfRequest(13));
  EXPECT_EQ(12, GetOrderOfRequest(14));
  EXPECT_EQ(13, GetOrderOfRequest(15));
  EXPECT_EQ(15, GetOrderOfRequest(16));

  // Make sure we test order of all requests made.
  EXPECT_EQ(ClientSocketPoolTest::kIndexOutOfBounds, GetOrderOfRequest(17));
}

class RequestSocketCallback : public CallbackRunner< Tuple1<int> > {
 public:
  RequestSocketCallback(ClientSocketHandle* handle,
                        TransportClientSocketPool* pool)
      : handle_(handle),
        pool_(pool),
        within_callback_(false) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    ASSERT_EQ(OK, params.a);

    if (!within_callback_) {
      // Don't allow reuse of the socket.  Disconnect it and then release it and
      // run through the MessageLoop once to get it completely released.
      handle_->socket()->Disconnect();
      handle_->Reset();
      {
        MessageLoop::ScopedNestableTaskAllower nestable(
            MessageLoop::current());
        MessageLoop::current()->RunAllPending();
      }
      within_callback_ = true;
      scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
          HostPortPair("www.google.com", 80), LOWEST, GURL(), false, false));
      int rv = handle_->Init("a", dest, LOWEST, this, pool_, BoundNetLog());
      EXPECT_EQ(OK, rv);
    }
  }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

 private:
  ClientSocketHandle* const handle_;
  TransportClientSocketPool* const pool_;
  bool within_callback_;
  TestCompletionCallback callback_;
};

// Disabled in release with dcheck : http://crbug.com/94501
#if defined(DCHECK_ALWAYS_ON)
#define MAYBE_RequestTwice DISABLED_RequestTwice
#else
#define MAYBE_RequestTwice RequestTwice
#endif
TEST_F(TransportClientSocketPoolTest, MAYBE_RequestTwice) {
  ClientSocketHandle handle;
  RequestSocketCallback callback(&handle, &pool_);
  scoped_refptr<TransportSocketParams> dest(new TransportSocketParams(
      HostPortPair("www.google.com", 80), LOWEST, GURL(), false, false));
  int rv = handle.Init("a", dest, LOWEST, &callback, &pool_,
                       BoundNetLog());
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // The callback is going to request "www.google.com". We want it to complete
  // synchronously this time.
  host_resolver_->set_synchronous_mode(true);

  EXPECT_EQ(OK, callback.WaitForResult());

  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(TransportClientSocketPoolTest, CancelActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET);

  // Queue up all the requests
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));
  EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  ASSERT_LE(kMaxSocketsPerGroup, static_cast<int>(requests()->size()));
  for (int i = 0; i < kMaxSocketsPerGroup; i++)
    (*requests())[i]->handle()->Reset();

  // Let's wait for the rest to complete now.
  for (size_t i = kMaxSocketsPerGroup; i < requests()->size(); ++i) {
    EXPECT_EQ(OK, (*requests())[i]->WaitForResult());
    (*requests())[i]->handle()->Reset();
  }

  EXPECT_EQ(requests()->size() - kMaxSocketsPerGroup, completion_count());
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(TransportClientSocketPoolTest, FailingActiveRequestWithPendingRequests) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_PENDING_FAILING_CLIENT_SOCKET);

  const int kNumRequests = 2 * kMaxSocketsPerGroup + 1;
  ASSERT_LE(kNumRequests, kMaxSockets);  // Otherwise the test will hang.

  // Queue up all the requests
  for (int i = 0; i < kNumRequests; i++)
    EXPECT_EQ(ERR_IO_PENDING, StartRequest("a", kDefaultPriority));

  for (int i = 0; i < kNumRequests; i++)
    EXPECT_EQ(ERR_CONNECTION_FAILED, (*requests())[i]->WaitForResult());
}

TEST_F(TransportClientSocketPoolTest, ResetIdleSocketsOnIPAddressChange) {
  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());

  handle.Reset();

  // Need to run all pending to release the socket back to the pool.
  MessageLoop::current()->RunAllPending();

  // Now we should have 1 idle socket.
  EXPECT_EQ(1, pool_.IdleSocketCount());

  // After an IP address change, we should have 0 idle sockets.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChangeForTests();
  MessageLoop::current()->RunAllPending();  // Notification happens async.

  EXPECT_EQ(0, pool_.IdleSocketCount());
}

TEST_F(TransportClientSocketPoolTest, BackupSocketConnect) {
  // Case 1 tests the first socket stalling, and the backup connecting.
  MockClientSocketFactory::ClientSocketType case1_types[] = {
    // The first socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // The second socket will connect more quickly.
    MockClientSocketFactory::MOCK_CLIENT_SOCKET
  };

  // Case 2 tests the first socket being slow, so that we start the
  // second connect, but the second connect stalls, and we still
  // complete the first.
  MockClientSocketFactory::ClientSocketType case2_types[] = {
    // The first socket will connect, although delayed.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // The second socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET
  };

  MockClientSocketFactory::ClientSocketType* cases[2] = {
    case1_types,
    case2_types
  };

  for (size_t index = 0; index < arraysize(cases); ++index) {
    client_socket_factory_.set_client_socket_types(cases[index], 2);

    EXPECT_EQ(0, pool_.IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv = handle.Init("b", low_params_, LOW, &callback, &pool_,
                         BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    MessageLoop::current()->RunAllPending();

    // Wait for the backup socket timer to fire.
    base::PlatformThread::Sleep(
        ClientSocketPool::kMaxConnectRetryIntervalMs + 50);

    // Let the appropriate socket connect.
    MessageLoop::current()->RunAllPending();

    EXPECT_EQ(OK, callback.WaitForResult());
    EXPECT_TRUE(handle.is_initialized());
    EXPECT_TRUE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_.IdleSocketCount());
    handle.Reset();

    // Close all pending connect jobs and existing sockets.
    pool_.Flush();
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket, but then we cancelled the request after that.
TEST_F(TransportClientSocketPoolTest, BackupSocketCancel) {
  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET);

  enum { CANCEL_BEFORE_WAIT, CANCEL_AFTER_WAIT };

  for (int index = CANCEL_BEFORE_WAIT; index < CANCEL_AFTER_WAIT; ++index) {
    EXPECT_EQ(0, pool_.IdleSocketCount());

    TestCompletionCallback callback;
    ClientSocketHandle handle;
    int rv = handle.Init("c", low_params_, LOW, &callback, &pool_,
                         BoundNetLog());
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // Create the first socket, set the timer.
    MessageLoop::current()->RunAllPending();

    if (index == CANCEL_AFTER_WAIT) {
      // Wait for the backup socket timer to fire.
      base::PlatformThread::Sleep(ClientSocketPool::kMaxConnectRetryIntervalMs);
    }

    // Let the appropriate socket connect.
    MessageLoop::current()->RunAllPending();

    handle.Reset();

    EXPECT_FALSE(callback.have_result());
    EXPECT_FALSE(handle.is_initialized());
    EXPECT_FALSE(handle.socket());

    // One socket is stalled, the other is active.
    EXPECT_EQ(0, pool_.IdleSocketCount());
  }
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and never completes, and then the backup
// connection fails.
//
// Flaky on Mac + Linux - http://crbug.com/86550
// Flaky on ChromeOS - http://crbug.com/89273
// Disabled in release with dcheck : http://crbug.com/94501
#if defined(OS_MACOSX) || defined(OS_CHROMEOS) || defined(OS_LINUX)
#define MAYBE_BackupSocketFailAfterStall FLAKY_BackupSocketFailAfterStall
#elif defined(DCHECK_ALWAYS_ON)
#define MAYBE_BackupSocketFailAfterStall DISABLED_BackupSocketFailAfterStall
#else
#define MAYBE_BackupSocketFailAfterStall BackupSocketFailAfterStall
#endif
TEST_F(TransportClientSocketPoolTest, MAYBE_BackupSocketFailAfterStall) {
  MockClientSocketFactory::ClientSocketType case_types[] = {
    // The first socket will not connect.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // The second socket will fail immediately.
    MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);

  EXPECT_EQ(0, pool_.IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("b", low_params_, LOW, &callback, &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  MessageLoop::current()->RunAllPending();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(ClientSocketPool::kMaxConnectRetryIntervalMs);

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  host_resolver_->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  EXPECT_EQ(0, pool_.IdleSocketCount());
  handle.Reset();

  // Reset for the next case.
  host_resolver_->set_synchronous_mode(false);
}

// Test the case where a socket took long enough to start the creation
// of the backup socket and eventually completes, but the backup socket
// fails.
// Disabled in release with dcheck : http://crbug.com/94501
#if defined(DCHECK_ALWAYS_ON)
#define MAYBE_BackupSocketFailAfterDelay DISABLED_BackupSocketFailAfterDelay
#else
#define MAYBE_BackupSocketFailAfterDelay BackupSocketFailAfterDelay
#endif
TEST_F(TransportClientSocketPoolTest, MAYBE_BackupSocketFailAfterDelay) {
  MockClientSocketFactory::ClientSocketType case_types[] = {
    // The first socket will connect, although delayed.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // The second socket will not connect.
    MockClientSocketFactory::MOCK_FAILING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);
  client_socket_factory_.set_delay_ms(5000);

  EXPECT_EQ(0, pool_.IdleSocketCount());

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("b", low_params_, LOW, &callback, &pool_, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  // Create the first socket, set the timer.
  MessageLoop::current()->RunAllPending();

  // Wait for the backup socket timer to fire.
  base::PlatformThread::Sleep(ClientSocketPool::kMaxConnectRetryIntervalMs);

  // Let the second connect be synchronous. Otherwise, the emulated
  // host resolution takes an extra trip through the message loop.
  host_resolver_->set_synchronous_mode(true);

  // Let the appropriate socket connect.
  MessageLoop::current()->RunAllPending();

  EXPECT_EQ(ERR_CONNECTION_FAILED, callback.WaitForResult());
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());
  handle.Reset();

  // Reset for the next case.
  host_resolver_->set_synchronous_mode(false);
}

// Test the case of the IPv6 address stalling, and falling back to the IPv4
// socket which finishes first.
TEST_F(TransportClientSocketPoolTest, IPv6FallbackSocketIPv4FinishesFirst) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  MockClientSocketFactory::ClientSocketType case_types[] = {
    // This is the IPv6 socket.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET,
    // This is the IPv4 socket.
    MockClientSocketFactory::MOCK_PENDING_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,2.2.2.2", "");

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv4AddressSize, endpoint.address().size());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

// Test the case of the IPv6 address being slow, thus falling back to trying to
// connect to the IPv4 address, but having the connect to the IPv6 address
// finish first.
TEST_F(TransportClientSocketPoolTest, IPv6FallbackSocketIPv6FinishesFirst) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  MockClientSocketFactory::ClientSocketType case_types[] = {
    // This is the IPv6 socket.
    MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET,
    // This is the IPv4 socket.
    MockClientSocketFactory::MOCK_STALLED_CLIENT_SOCKET
  };

  client_socket_factory_.set_client_socket_types(case_types, 2);
  client_socket_factory_.set_delay_ms(
      TransportConnectJob::kIPv6FallbackTimerInMs + 50);

  // Resolve an AddressList with a IPv6 address first and then a IPv4 address.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,2.2.2.2", "");

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv6AddressSize, endpoint.address().size());
  EXPECT_EQ(2, client_socket_factory_.allocation_count());
}

TEST_F(TransportClientSocketPoolTest, IPv6NoIPv4AddressesToFallbackTo) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv6 addresses.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "2:abcd::3:4:ff,3:abcd::3:4:ff", "");

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv6AddressSize, endpoint.address().size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

TEST_F(TransportClientSocketPoolTest, IPv4HasNoFallback) {
  // Create a pool without backup jobs.
  ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(false);
  TransportClientSocketPool pool(kMaxSockets,
                                 kMaxSocketsPerGroup,
                                 histograms_.get(),
                                 host_resolver_.get(),
                                 &client_socket_factory_,
                                 NULL);

  client_socket_factory_.set_client_socket_type(
      MockClientSocketFactory::MOCK_DELAYED_CLIENT_SOCKET);

  // Resolve an AddressList with only IPv4 addresses.
  host_resolver_->rules()->AddIPLiteralRule(
      "*", "1.1.1.1", "");

  TestCompletionCallback callback;
  ClientSocketHandle handle;
  int rv = handle.Init("a", low_params_, LOW, &callback, &pool, BoundNetLog());
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(handle.is_initialized());
  EXPECT_FALSE(handle.socket());

  EXPECT_EQ(OK, callback.WaitForResult());
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  IPEndPoint endpoint;
  handle.socket()->GetLocalAddress(&endpoint);
  EXPECT_EQ(kIPv4AddressSize, endpoint.address().size());
  EXPECT_EQ(1, client_socket_factory_.allocation_count());
}

}  // namespace

}  // namespace net
