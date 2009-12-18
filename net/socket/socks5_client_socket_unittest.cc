// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socks5_client_socket.h"

#include <algorithm>
#include <map>

#include "net/base/address_list.h"
#include "net/base/load_log.h"
#include "net/base/load_log_unittest.h"
#include "net/base/mock_host_resolver.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/base/winsock_init.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

namespace {

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
      HostResolver::RequestInfo(hostname, port));
}

const char kSOCKS5GreetRequest[] = { 0x05, 0x01, 0x00 };
const char kSOCKS5GreetResponse[] = { 0x05, 0x00 };
const char kSOCKS5OkResponse[] =
    { 0x05, 0x00, 0x00, 0x01, 127, 0, 0, 1, 0x00, 0x50 };


// Tests a complete SOCKS5 handshake and the disconnection.
TEST_F(SOCKS5ClientSocketTest, CompleteHandshake) {
  const std::string payload_write = "random data";
  const std::string payload_read = "moar random data";

  const char kSOCKS5OkRequest[] = {
    0x05,  // Version
    0x01,  // Command (CONNECT)
    0x00,  // Reserved.
    0x03,  // Address type (DOMAINNAME).
    0x09,  // Length of domain (9)
    // Domain string:
    'l', 'o', 'c', 'a', 'l', 'h', 'o', 's', 't',
    0x00, 0x50,  // 16-bit port (80)
  };

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

// Test that you can call Connect() again after having called Disconnect().
TEST_F(SOCKS5ClientSocketTest, ConnectAndDisconnectTwice) {
  const std::string hostname = "my-host-name";
  const char kSOCKS5DomainRequest[] = {
      0x05,  // VER
      0x01,  // CMD
      0x00,  // RSV
      0x03,  // ATYPE
  };

  std::string request(kSOCKS5DomainRequest, arraysize(kSOCKS5DomainRequest));
  request.push_back(hostname.size());
  request.append(hostname);
  request.append(reinterpret_cast<const char*>(&kNwPort), sizeof(kNwPort));

  for (int i = 0; i < 2; ++i) {
    MockWrite data_writes[] = {
        MockWrite(false, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(false, request.data(), request.size())
    };
    MockRead data_reads[] = {
        MockRead(false, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
        MockRead(false, kSOCKS5OkResponse, arraysize(kSOCKS5OkResponse))
    };

    user_sock_.reset(BuildMockSocket(data_reads, data_writes, hostname, 80));

    int rv = user_sock_->Connect(&callback_, NULL);
    EXPECT_EQ(OK, rv);
    EXPECT_TRUE(user_sock_->IsConnected());

    user_sock_->Disconnect();
    EXPECT_FALSE(user_sock_->IsConnected());
  }
}

// Test that we fail trying to connect to a hosname longer than 255 bytes.
TEST_F(SOCKS5ClientSocketTest, LargeHostNameFails) {
  // Create a string of length 256, where each character is 'x'.
  std::string large_host_name;
  std::fill_n(std::back_inserter(large_host_name), 256, 'x');

  // Create a SOCKS socket, with mock transport socket.
  MockWrite data_writes[] = {MockWrite()};
  MockRead data_reads[] = {MockRead()};
  user_sock_.reset(BuildMockSocket(data_reads, data_writes,
                                   large_host_name, 80));

  // Try to connect -- should fail (without having read/written anything to
  // the transport socket first) because the hostname is too long.
  TestCompletionCallback callback;
  int rv = user_sock_->Connect(&callback, NULL);
  EXPECT_EQ(ERR_INVALID_URL, rv);
}

TEST_F(SOCKS5ClientSocketTest, PartialReadWrites) {
  const std::string hostname = "www.google.com";

  const char kSOCKS5OkRequest[] = {
    0x05,  // Version
    0x01,  // Command (CONNECT)
    0x00,  // Reserved.
    0x03,  // Address type (DOMAINNAME).
    0x0E,  // Length of domain (14)
    // Domain string:
    'w', 'w', 'w', '.', 'g', 'o', 'o', 'g', 'l', 'e', '.', 'c', 'o', 'm',
    0x00, 0x50,  // 16-bit port (80)
  };

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

  // Test for partial handshake request write.
  {
    const int kSplitPoint = 3;  // Break handshake write into two parts.
    MockWrite data_writes[] = {
        MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(true, kSOCKS5OkRequest, kSplitPoint),
        MockWrite(true, kSOCKS5OkRequest + kSplitPoint,
                        arraysize(kSOCKS5OkRequest) - kSplitPoint)
    };
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
    const int kSplitPoint = 6;  // Break the handshake read into two parts.
    MockWrite data_writes[] = {
        MockWrite(true, kSOCKS5GreetRequest, arraysize(kSOCKS5GreetRequest)),
        MockWrite(true, kSOCKS5OkRequest, arraysize(kSOCKS5OkRequest))
    };
    MockRead data_reads[] = {
        MockRead(true, kSOCKS5GreetResponse, arraysize(kSOCKS5GreetResponse)),
        MockRead(true, kSOCKS5OkResponse, kSplitPoint),
        MockRead(true, kSOCKS5OkResponse + kSplitPoint, arraysize(kSOCKS5OkResponse) - kSplitPoint)
    };

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

}  // namespace

}  // namespace net
