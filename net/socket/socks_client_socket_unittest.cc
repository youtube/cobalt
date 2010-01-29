// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks_client_socket.h"

#include "net/base/address_list.h"
#include "net/base/load_log.h"
#include "net/base/load_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/tcp_client_socket.h"
#include "net/socket/socket_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

const char kSOCKSOkRequest[] = { 0x04, 0x01, 0x00, 0x50, 127, 0, 0, 1, 0 };
const char kSOCKS4aInitialRequest[] =
    { 0x04, 0x01, 0x00, 0x50, 0, 0, 0, 127, 0 };
const char kSOCKSOkReply[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

class SOCKSClientSocketTest : public PlatformTest {
 public:
  SOCKSClientSocketTest();
  // Create a SOCKSClientSocket on top of a MockSocket.
  SOCKSClientSocket* BuildMockSocket(MockRead reads[], MockWrite writes[],
                                     HostResolver* host_resolver,
                                     const std::string& hostname, int port);
  virtual void SetUp();

 protected:
  scoped_ptr<SOCKSClientSocket> user_sock_;
  AddressList address_list_;
  ClientSocket* tcp_sock_;
  TestCompletionCallback callback_;
  scoped_refptr<MockHostResolver> host_resolver_;
  scoped_ptr<SocketDataProvider> data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SOCKSClientSocketTest);
};

SOCKSClientSocketTest::SOCKSClientSocketTest()
  : host_resolver_(new MockHostResolver) {
}

// Set up platform before every test case
void SOCKSClientSocketTest::SetUp() {
  PlatformTest::SetUp();
}

SOCKSClientSocket* SOCKSClientSocketTest::BuildMockSocket(
    MockRead reads[],
    MockWrite writes[],
    HostResolver* host_resolver,
    const std::string& hostname,
    int port) {

  TestCompletionCallback callback;
  data_.reset(new StaticSocketDataProvider(reads, writes));
  tcp_sock_ = new MockTCPClientSocket(address_list_, data_.get());

  int rv = tcp_sock_->Connect(&callback, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(tcp_sock_->IsConnected());

  return new SOCKSClientSocket(tcp_sock_,
      HostResolver::RequestInfo(hostname, port),
      host_resolver);
}

// Implementation of HostResolver that never completes its resolve request.
// We use this in the test "DisconnectWhileHostResolveInProgress" to make
// sure that the outstanding resolve request gets cancelled.
class HangingHostResolver : public HostResolver {
 public:
  HangingHostResolver() : outstanding_request_(NULL) {}

  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      LoadLog* load_log) {
    EXPECT_FALSE(HasOutstandingRequest());
    outstanding_request_ = reinterpret_cast<RequestHandle>(1);
    *out_req = outstanding_request_;
    return ERR_IO_PENDING;
  }

  virtual void CancelRequest(RequestHandle req) {
    EXPECT_TRUE(HasOutstandingRequest());
    EXPECT_EQ(outstanding_request_, req);
    outstanding_request_ = NULL;
  }

  virtual void AddObserver(Observer* observer) {}
  virtual void RemoveObserver(Observer* observer) {}
  virtual HostCache* GetHostCache() { return NULL; }
  virtual void Shutdown() {}

  bool HasOutstandingRequest() {
    return outstanding_request_ != NULL;
  }

 private:
  RequestHandle outstanding_request_;

  DISALLOW_COPY_AND_ASSIGN(HangingHostResolver);
};

// Tests a complete handshake and the disconnection.
TEST_F(SOCKSClientSocketTest, CompleteHandshake) {
  const std::string payload_write = "random data";
  const std::string payload_read = "moar random data";

  MockWrite data_writes[] = {
      MockWrite(true, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)),
      MockWrite(true, payload_write.data(), payload_write.size()) };
  MockRead data_reads[] = {
      MockRead(true, kSOCKSOkReply, arraysize(kSOCKSOkReply)),
      MockRead(true, payload_read.data(), payload_read.size()) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   "localhost", 80));

  // At this state the TCP connection is completed but not the SOCKS handshake.
  EXPECT_TRUE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());

  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(
      LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  EXPECT_FALSE(user_sock_->IsConnected());
  rv = callback_.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKSClientSocket::kSOCKS4, user_sock_->socks_version_);
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));

  scoped_refptr<IOBuffer> buffer = new IOBuffer(payload_write.size());
  memcpy(buffer->data(), payload_write.data(), payload_write.size());
  rv = user_sock_->Write(buffer, payload_write.size(), &callback_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_.WaitForResult();
  EXPECT_EQ(static_cast<int>(payload_write.size()), rv);

  buffer = new IOBuffer(payload_read.size());
  rv = user_sock_->Read(buffer, payload_read.size(), &callback_);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback_.WaitForResult();
  EXPECT_EQ(static_cast<int>(payload_read.size()), rv);
  EXPECT_EQ(payload_read, std::string(buffer->data(), payload_read.size()));

  user_sock_->Disconnect();
  EXPECT_FALSE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());
}

// List of responses from the socks server and the errors they should
// throw up are tested here.
TEST_F(SOCKSClientSocketTest, HandshakeFailures) {
  const struct {
    const char fail_reply[8];
    Error fail_code;
  } tests[] = {
    // Failure of the server response code
    {
      { 0x01, 0x5A, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_INVALID_RESPONSE,
    },
    // Failure of the null byte
    {
      { 0x00, 0x5B, 0x00, 0x00, 0, 0, 0, 0 },
      ERR_FAILED,
    },
  };

  //---------------------------------------

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    MockWrite data_writes[] = {
        MockWrite(false, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
    MockRead data_reads[] = {
        MockRead(false, tests[i].fail_reply, arraysize(tests[i].fail_reply)) };

    user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                     "localhost", 80));
    scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

    int rv = user_sock_->Connect(&callback_, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
    rv = callback_.WaitForResult();
    EXPECT_EQ(tests[i].fail_code, rv);
    EXPECT_FALSE(user_sock_->IsConnected());
    EXPECT_TRUE(tcp_sock_->IsConnected());
    EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
  }
}

// Tests scenario when the server sends the handshake response in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialServerReads) {
  const char kSOCKSPartialReply1[] = { 0x00 };
  const char kSOCKSPartialReply2[] = { 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
      MockWrite(true, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
  MockRead data_reads[] = {
      MockRead(true, kSOCKSPartialReply1, arraysize(kSOCKSPartialReply1)),
      MockRead(true, kSOCKSPartialReply2, arraysize(kSOCKSPartialReply2)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   "localhost", 80));
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContainsBeginEvent(
      *log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
}

// Tests scenario when the client sends the handshake request in
// more than one packet.
TEST_F(SOCKSClientSocketTest, PartialClientWrites) {
  const char kSOCKSPartialRequest1[] = { 0x04, 0x01 };
  const char kSOCKSPartialRequest2[] = { 0x00, 0x50, 127, 0, 0, 1, 0 };

  MockWrite data_writes[] = {
      MockWrite(true, arraysize(kSOCKSPartialRequest1)),
      // simulate some empty writes
      MockWrite(true, 0),
      MockWrite(true, 0),
      MockWrite(true, kSOCKSPartialRequest2,
                arraysize(kSOCKSPartialRequest2)) };
  MockRead data_reads[] = {
      MockRead(true, kSOCKSOkReply, arraysize(kSOCKSOkReply)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   "localhost", 80));
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
}

// Tests the case when the server sends a smaller sized handshake data
// and closes the connection.
TEST_F(SOCKSClientSocketTest, FailedSocketRead) {
  MockWrite data_writes[] = {
      MockWrite(true, kSOCKSOkRequest, arraysize(kSOCKSOkRequest)) };
  MockRead data_reads[] = {
      MockRead(true, kSOCKSOkReply, arraysize(kSOCKSOkReply) - 2),
      // close connection unexpectedly
      MockRead(false, 0) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   "localhost", 80));
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  rv = callback_.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_CLOSED, rv);
  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
}

// Tries to connect to an unknown DNS and on failure should revert to SOCKS4A.
TEST_F(SOCKSClientSocketTest, SOCKS4AFailedDNS) {
  const char hostname[] = "unresolved.ipv4.address";

  host_resolver_->rules()->AddSimulatedFailure(hostname);

  std::string request(kSOCKS4aInitialRequest,
                      arraysize(kSOCKS4aInitialRequest));
  request.append(hostname, arraysize(hostname));

  MockWrite data_writes[] = {
      MockWrite(false, request.data(), request.size()) };
  MockRead data_reads[] = {
      MockRead(false, kSOCKSOkReply, arraysize(kSOCKSOkReply)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   hostname, 80));
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKSClientSocket::kSOCKS4a, user_sock_->socks_version_);
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
}

// Tries to connect to a domain that resolves to IPv6.
// Should revert to SOCKS4a.
TEST_F(SOCKSClientSocketTest, SOCKS4AIfDomainInIPv6) {
  const char hostname[] = "an.ipv6.address";

  host_resolver_->rules()->AddIPv6Rule(hostname, "2001:db8:8714:3a90::12");

  std::string request(kSOCKS4aInitialRequest,
                      arraysize(kSOCKS4aInitialRequest));
  request.append(hostname, arraysize(hostname));

  MockWrite data_writes[] = {
      MockWrite(false, request.data(), request.size()) };
  MockRead data_reads[] = {
      MockRead(false, kSOCKSOkReply, arraysize(kSOCKSOkReply)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, host_resolver_,
                                   hostname, 80));
  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));

  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContainsBeginEvent(*log, 0, LoadLog::TYPE_SOCKS_CONNECT));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKSClientSocket::kSOCKS4a, user_sock_->socks_version_);
  EXPECT_TRUE(LogContainsEndEvent(*log, -1, LoadLog::TYPE_SOCKS_CONNECT));
}

// Calls Disconnect() while a host resolve is in progress. The outstanding host
// resolve should be cancelled.
TEST_F(SOCKSClientSocketTest, DisconnectWhileHostResolveInProgress) {
  scoped_refptr<HangingHostResolver> hanging_resolver =
      new HangingHostResolver();

  // Doesn't matter what the socket data is, we will never use it -- garbage.
  MockWrite data_writes[] = { MockWrite(false, "", 0) };
  MockRead data_reads[] = { MockRead(false, "", 0) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, hanging_resolver,
                                   "foo", 80));

  // Start connecting (will get stuck waiting for the host to resolve).
  int rv = user_sock_->Connect(&callback_, NULL);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());

  // The host resolver should have received the resolve request.
  EXPECT_TRUE(hanging_resolver->HasOutstandingRequest());

  // Disconnect the SOCKS socket -- this should cancel the outstanding resolve.
  user_sock_->Disconnect();

  EXPECT_FALSE(hanging_resolver->HasOutstandingRequest());

  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnectedAndIdle());
}

}  // namespace net
