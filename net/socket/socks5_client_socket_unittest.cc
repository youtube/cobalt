// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks5_client_socket.h"

#include <map>
#include "build/build_config.h"
#if defined(OS_WIN)
#include <ws2tcpip.h>
#elif defined(OS_POSIX)
#include <netdb.h>
#endif
#include "net/base/address_list.h"
#include "net/base/load_log.h"
#include "net/base/load_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

// Base class to test SOCKS5ClientSocket
class SOCKS5ClientSocketTest : public PlatformTest {
 public:
  SOCKS5ClientSocketTest();
  // Create a SOCKSClientSocket on top of a MockSocket.
  SOCKS5ClientSocket* BuildMockSocket(MockRead reads[],
                                      MockWrite writes[],
                                      const std::string& hostname,
                                      int port);
  virtual void SetUp();

 protected:
  const uint16 kNwPort;
  scoped_ptr<SOCKS5ClientSocket> user_sock_;
  AddressList address_list_;
  ClientSocket* tcp_sock_;
  TestCompletionCallback callback_;
  scoped_refptr<MockHostResolver> host_resolver_;
  scoped_ptr<SocketDataProvider> data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SOCKS5ClientSocketTest);
};

SOCKS5ClientSocketTest::SOCKS5ClientSocketTest()
  : kNwPort(htons(80)), host_resolver_(new MockHostResolver) {
}

// Set up platform before every test case
void SOCKS5ClientSocketTest::SetUp() {
  PlatformTest::SetUp();

  // Resolve the "localhost" AddressList used by the TCP connection to connect.
  HostResolver::RequestInfo info("www.socks-proxy.com", 1080);
  int rv = host_resolver_->Resolve(info, &address_list_, NULL, NULL, NULL);
  ASSERT_EQ(OK, rv);
}

SOCKS5ClientSocket* SOCKS5ClientSocketTest::BuildMockSocket(
    MockRead reads[],
    MockWrite writes[],
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

  return new SOCKS5ClientSocket(tcp_sock_,
      HostResolver::RequestInfo(hostname, port),
      host_resolver_);
}

const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };

const char kSOCKS5OkRequest[] =
    { 0x05, 0x01, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };
const char kSOCKS5OkResponse[] =
    { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };

// Tests a complete SOCKS5 handshake and the disconnection.
TEST_F(SOCKS5ClientSocketTest, CompleteHandshake) {
  const std::string payload_write = "random data";
  const std::string payload_read = "moar random data";

  MockWrite data_writes[] = {
      MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
      MockWrite(true, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest)),
      MockWrite(true, payload_write.data(), payload_write.size()) };
  MockRead data_reads[] = {
      MockRead(true, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
      MockRead(true, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)),
      MockRead(true, payload_read.data(), payload_read.size()) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, "localhost", 80));

  // At this state the TCP connection is completed but not the SOCKS handshake.
  EXPECT_TRUE(tcp_sock_->IsConnected());
  EXPECT_FALSE(user_sock_->IsConnected());

  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_FALSE(user_sock_->IsConnected());
  EXPECT_TRUE(
      LogContains(*log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));

  rv = callback_.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKS5ClientSocket::kEndPointResolvedIPv4,
            user_sock_->address_type_);
  EXPECT_TRUE(LogContains(
      *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));

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

// Tries to connect to a DNS which fails domain lookup.
TEST_F(SOCKS5ClientSocketTest, FailedDNS) {
  const std::string hostname = "unresolved.ipv4.address";
  const char kSOCKS5DomainRequest[] = { 0x05, 0x01, 0x00, 0x03 };

  host_resolver_->rules()->AddSimulatedFailure(hostname.c_str());

  std::string request(kSOCKS5DomainRequest,
                      arraysize(kSOCKS5DomainRequest));
  request.push_back(hostname.size());
  request.append(hostname);
  request.append(reinterpret_cast<const char*>(&kNwPort), sizeof(kNwPort));

  MockWrite data_writes[] = {
      MockWrite(false, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
      MockWrite(false, request.data(), request.size()) };
  MockRead data_reads[] = {
      MockRead(false, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
      MockRead(false, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));

  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContains(
      *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKS5ClientSocket::kEndPointFailedDomain,
            user_sock_->address_type_);
  EXPECT_TRUE(LogContains(
      *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
}

// Tries to connect to a domain that resolves to IPv6.
TEST_F(SOCKS5ClientSocketTest, IPv6Domain) {
  const std::string hostname = "an.ipv6.address";
  const char kSOCKS5IPv6Request[] = { 0x05, 0x01, 0x00, 0x04 };
  const uint8 ipv6_addr[] = { 0x20, 0x01, 0x0d, 0xb8, 0x87, 0x14, 0x3a, 0x90,
                              0x00, 0x00, 0x00, 0x00, 0x00, 0x000, 0x00, 0x12 };

  host_resolver_->rules()->AddIPv6Rule(hostname, "2001:db8:8714:3a90::12");

  std::string request(kSOCKS5IPv6Request,
                      arraysize(kSOCKS5IPv6Request));
  request.append(reinterpret_cast<const char*>(&ipv6_addr), sizeof(ipv6_addr));
  request.append(reinterpret_cast<const char*>(&kNwPort), sizeof(kNwPort));

  MockWrite data_writes[] = {
      MockWrite(false, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
      MockWrite(false, request.data(), request.size()) };
  MockRead data_reads[] = {
      MockRead(false, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
      MockRead(false, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)) };

  user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));

  scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
  int rv = user_sock_->Connect(&callback_, log);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  EXPECT_TRUE(LogContains(
      *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
  rv = callback_.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(user_sock_->IsConnected());
  EXPECT_EQ(SOCKS5ClientSocket::kEndPointResolvedIPv6,
            user_sock_->address_type_);
  EXPECT_TRUE(LogContains(
      *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
}

TEST_F(SOCKS5ClientSocketTest, PartialReadWrites) {
  const std::string hostname = "www.google.com";

  // Test for partial greet request write
  {
    const char partial1[] = { 0x05, 0x01 };
    const char partial2[] = { 0x00 };
    MockWrite data_writes[] = {
        MockWrite(true, arraysize(partial1)),
        MockWrite(true, partial2, arraysize(partial2)),
        MockWrite(true, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest)) };
    MockRead data_reads[] = {
        MockRead(true, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
        MockRead(true, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)) };
    user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));
    scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
    int rv = user_sock_->Connect(&callback_, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(LogContains(
        *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
    rv = callback_.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(user_sock_->IsConnected());
    EXPECT_TRUE(LogContains(
        *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
  }

  // Test for partial greet response read
  {
    const char partial1[] = { 0x05 };
    const char partial2[] = { 0x00 };
    MockWrite data_writes[] = {
        MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(true, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest)) };
    MockRead data_reads[] = {
        MockRead(true, partial1, arraysize(partial1)),
        MockRead(true, partial2, arraysize(partial2)),
        MockRead(true, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)) };
    user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));
    scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
    int rv = user_sock_->Connect(&callback_, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(LogContains(
        *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
    rv = callback_.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(user_sock_->IsConnected());
    EXPECT_TRUE(LogContains(
        *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
  }

  // Test for partial handshake request write
  {
    const char partial1[] = { 0x05, 0x01, 0x00 };
    const char partial2[] = { 0x01, 127, 0, 0, 1, 0x00, 0x50 };
    MockWrite data_writes[] = {
        MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(true, arraysize(partial1)),
        MockWrite(true, partial2, arraysize(partial2)) };
    MockRead data_reads[] = {
        MockRead(true, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
        MockRead(true, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse)) };
    user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));
    scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
    int rv = user_sock_->Connect(&callback_, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(LogContains(
        *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
    rv = callback_.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(user_sock_->IsConnected());
    EXPECT_TRUE(LogContains(
        *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
  }

  // Test for partial handshake response read
  {
    const char partial1[] = { 0x05, 0x00, 0x00, 0x01, 127, 0 };
    const char partial2[] = { 0, 1, 0x00, 0x50 };
    MockWrite data_writes[] = {
        MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(true, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest)) };
    MockRead data_reads[] = {
        MockRead(true, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
        MockRead(true, partial1, arraysize(partial1)),
        MockRead(true, partial2, arraysize(partial2)) };
    user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));
    scoped_refptr<LoadLog> log(new LoadLog(LoadLog::kUnbounded));
    int rv = user_sock_->Connect(&callback_, log);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    EXPECT_TRUE(LogContains(
        *log, 0, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_BEGIN));
    rv = callback_.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(user_sock_->IsConnected());
    EXPECT_TRUE(LogContains(
        *log, -1, LoadLog::TYPE_SOCKS5_CONNECT, LoadLog::PHASE_END));
  }
}

}  // namespace net
