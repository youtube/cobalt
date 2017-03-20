// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_listen_socket_unittest.h"

#if !defined(OS_STARBOARD)
#include <fcntl.h>
#include <sys/types.h>
#endif

#include "base/bind.h"
#if !defined(OS_STARBOARD)
#include "base/posix/eintr_wrapper.h"
#endif
#include "base/sys_byteorder.h"
#include "net/base/net_util.h"
#include "testing/platform_test.h"
#if defined(OS_STARBOARD)
#include "starboard/socket.h"
#endif

namespace net {

namespace {
int SocketReceive(SocketDescriptor socket, char* buffer, int size) {
#if defined(OS_STARBOARD)
  return SbSocketReceiveFrom(socket, buffer, size, NULL);
#else  // defined(OS_STARBOARD)
  return HANDLE_EINTR(recv(socket, buffer, size, 0));
#endif  // defined(OS_STARBOARD)
}

int SocketSend(SocketDescriptor socket, const char* buffer, int size) {
#if defined(OS_STARBOARD)
  return SbSocketSendTo(socket, buffer, size, NULL);
#else  // defined(OS_STARBOARD)
  return HANDLE_EINTR(send(socket, buffer, size, 0));
#endif  // defined(OS_STARBOARD)
}
}  // namespace

const int TCPListenSocketTester::kTestPort = 9999;

static const int kReadBufSize = 1024;
static const char kHelloWorld[] = "HELLO, WORLD";
static const int kMaxQueueSize = 20;
static const char kLoopback[] = "127.0.0.1";
static const int kDefaultTimeoutMs = 5000;

TCPListenSocketTester::TCPListenSocketTester()
    : thread_(NULL),
      loop_(NULL),
      server_(NULL),
      connection_(NULL),
      cv_(&lock_) {
}

void TCPListenSocketTester::SetUp() {
  base::Thread::Options options;
  options.message_loop_type = MessageLoop::TYPE_IO;
  thread_.reset(new base::Thread("socketio_test"));
  thread_->StartWithOptions(options);
  loop_ = reinterpret_cast<MessageLoopForIO*>(thread_->message_loop());

  loop_->PostTask(FROM_HERE, base::Bind(
      &TCPListenSocketTester::Listen, this));

  // verify Listen succeeded
  NextAction();
  ASSERT_FALSE(server_ == NULL);
  ASSERT_EQ(ACTION_LISTEN, last_action_.type());

  // verify the connect/accept and setup test_socket_
#if defined(OS_STARBOARD)
  test_socket_ = SbSocketCreate(kSbSocketAddressTypeIpv4, kSbSocketProtocolTcp);
  ASSERT_NE(StreamListenSocket::kInvalidSocket, test_socket_);
  SbSocketAddress address = {0};
  address.type = kSbSocketAddressTypeIpv4;
  address.address[0] = 127;
  address.address[3] = 1;
  address.port = kTestPort;
  int ret = SbSocketConnect(test_socket_, &address);
#else   // defined(OS_STARBOARD)
  test_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  ASSERT_NE(StreamListenSocket::kInvalidSocket, test_socket_);
  struct sockaddr_in client;
  client.sin_family = AF_INET;
  client.sin_addr.s_addr = inet_addr(kLoopback);
  client.sin_port = base::HostToNet16(kTestPort);
  int ret = HANDLE_EINTR(
      connect(test_socket_, reinterpret_cast<sockaddr*>(&client),
              sizeof(client)));
#endif  // defined(OS_STARBOARD)
#if defined(OS_POSIX)
  // The connect() call may be interrupted by a signal. When connect()
  // is retried on EINTR, it fails with EISCONN.
  if (ret == StreamListenSocket::kSocketError)
    ASSERT_EQ(EISCONN, errno);
#else
  // Don't have signals.
  ASSERT_NE(StreamListenSocket::kSocketError, ret);
#endif

  NextAction();
  ASSERT_EQ(ACTION_ACCEPT, last_action_.type());
}

void TCPListenSocketTester::TearDown() {
#if defined(OS_WIN)
  ASSERT_EQ(0, closesocket(test_socket_));
#elif defined(OS_POSIX)
  ASSERT_EQ(0, HANDLE_EINTR(close(test_socket_)));
#elif defined(OS_STARBOARD)
  ASSERT_EQ(true, SbSocketDestroy(test_socket_));
#endif
  NextAction();
  ASSERT_EQ(ACTION_CLOSE, last_action_.type());

  loop_->PostTask(FROM_HERE, base::Bind(
      &TCPListenSocketTester::Shutdown, this));
  NextAction();
  ASSERT_EQ(ACTION_SHUTDOWN, last_action_.type());

  thread_.reset();
  loop_ = NULL;
}

void TCPListenSocketTester::ReportAction(
    const TCPListenSocketTestAction& action) {
  base::AutoLock locked(lock_);
  queue_.push_back(action);
  cv_.Broadcast();
}

void TCPListenSocketTester::NextAction() {
  base::AutoLock locked(lock_);
  while (queue_.empty())
    cv_.Wait();
  last_action_ = queue_.front();
  queue_.pop_front();
}

int TCPListenSocketTester::ClearTestSocket() {
  char buf[kReadBufSize];
  int len_ret = 0;
  do {
    int len = SocketReceive(test_socket_, buf, kReadBufSize);
    if (len == StreamListenSocket::kSocketError || len == 0) {
      break;
    } else {
      len_ret += len;
    }
  } while (true);
  return len_ret;
}

void TCPListenSocketTester::Shutdown() {
  connection_->Release();
  connection_ = NULL;
  server_->Release();
  server_ = NULL;
  ReportAction(TCPListenSocketTestAction(ACTION_SHUTDOWN));
}

void TCPListenSocketTester::Listen() {
  server_ = DoListen();
  ASSERT_TRUE(server_);
  server_->AddRef();
  ReportAction(TCPListenSocketTestAction(ACTION_LISTEN));
}

void TCPListenSocketTester::SendFromTester() {
  connection_->Send(kHelloWorld);
  ReportAction(TCPListenSocketTestAction(ACTION_SEND));
}

void TCPListenSocketTester::TestClientSend() {
  ASSERT_TRUE(Send(test_socket_, kHelloWorld));
  NextAction();
  ASSERT_EQ(ACTION_READ, last_action_.type());
  ASSERT_EQ(last_action_.data(), kHelloWorld);
}

void TCPListenSocketTester::TestClientSendLong() {
  size_t hello_len = strlen(kHelloWorld);
  std::string long_string;
  size_t long_len = 0;
  for (int i = 0; i < 200; i++) {
    long_string += kHelloWorld;
    long_len += hello_len;
  }
  ASSERT_TRUE(Send(test_socket_, long_string));
  size_t read_len = 0;
  while (read_len < long_len) {
    NextAction();
    ASSERT_EQ(ACTION_READ, last_action_.type());
    std::string last_data = last_action_.data();
    size_t len = last_data.length();
    if (long_string.compare(read_len, len, last_data)) {
      ASSERT_EQ(long_string.compare(read_len, len, last_data), 0);
    }
    read_len += last_data.length();
  }
  ASSERT_EQ(read_len, long_len);
}

void TCPListenSocketTester::TestServerSend() {
  loop_->PostTask(FROM_HERE, base::Bind(
      &TCPListenSocketTester::SendFromTester, this));
  NextAction();
  ASSERT_EQ(ACTION_SEND, last_action_.type());
  const int buf_len = 200;
  char buf[buf_len+1];
  unsigned recv_len = 0;
  while (recv_len < strlen(kHelloWorld)) {
    int r = SocketReceive(test_socket_, buf + recv_len, buf_len - recv_len);
    ASSERT_GE(r, 0);
    recv_len += static_cast<unsigned>(r);
    if (!r)
      break;
  }
  buf[recv_len] = 0;
  ASSERT_STREQ(kHelloWorld, buf);
}

void TCPListenSocketTester::TestServerSendMultiple() {
  // Send enough data to exceed the socket receive window. 20kb is probably a
  // safe bet.
  int send_count = (1024*20) / (sizeof(kHelloWorld)-1);

  // Send multiple writes. Since no reading is occurring the data should be
  // buffered in TCPListenSocket.
  for (int i = 0; i < send_count; ++i) {
    loop_->PostTask(FROM_HERE, base::Bind(
        &TCPListenSocketTester::SendFromTester, this));
    NextAction();
    ASSERT_EQ(ACTION_SEND, last_action_.type());
  }

  // Make multiple reads. All of the data should eventually be returned.
  char buf[sizeof(kHelloWorld)];
  const int buf_len = sizeof(kHelloWorld);
  for (int i = 0; i < send_count; ++i) {
    unsigned recv_len = 0;
    while (recv_len < buf_len-1) {
      int r =
          SocketReceive(test_socket_, buf + recv_len, buf_len - 1 - recv_len);
      ASSERT_GE(r, 0);
      recv_len += static_cast<unsigned>(r);
      if (!r)
        break;
    }
    buf[recv_len] = 0;
    ASSERT_STREQ(kHelloWorld, buf);
  }
}

bool TCPListenSocketTester::Send(SocketDescriptor sock,
                                 const std::string& str) {
  int len = static_cast<int>(str.length());
  int send_len = SocketSend(sock, str.data(), len);

  if (send_len == StreamListenSocket::kSocketError) {
    PLOG(ERROR) << "send failed";
    return false;
  } else if (send_len != len) {
    return false;
  }
  return true;
}

void TCPListenSocketTester::DidAccept(StreamListenSocket* server,
                                      StreamListenSocket* connection) {
  connection_ = connection;
  connection_->AddRef();
  ReportAction(TCPListenSocketTestAction(ACTION_ACCEPT));
}

void TCPListenSocketTester::DidRead(StreamListenSocket* connection,
                                    const char* data,
                                    int len) {
  std::string str(data, len);
  ReportAction(TCPListenSocketTestAction(ACTION_READ, str));
}

void TCPListenSocketTester::DidClose(StreamListenSocket* sock) {
  ReportAction(TCPListenSocketTestAction(ACTION_CLOSE));
}

TCPListenSocketTester::~TCPListenSocketTester() {}

scoped_refptr<TCPListenSocket> TCPListenSocketTester::DoListen() {
  return TCPListenSocket::CreateAndListen(kLoopback, kTestPort, this);
}

class TCPListenSocketTest: public PlatformTest {
 public:
  TCPListenSocketTest() {
    tester_ = NULL;
  }

  virtual void SetUp() {
    PlatformTest::SetUp();
    tester_ = new TCPListenSocketTester();
    tester_->SetUp();
  }

  virtual void TearDown() {
    PlatformTest::TearDown();
    tester_->TearDown();
    tester_ = NULL;
  }

  scoped_refptr<TCPListenSocketTester> tester_;
};

TEST_F(TCPListenSocketTest, ClientSend) {
  tester_->TestClientSend();
}

TEST_F(TCPListenSocketTest, ClientSendLong) {
  tester_->TestClientSendLong();
}

TEST_F(TCPListenSocketTest, ServerSend) {
  tester_->TestServerSend();
}

TEST_F(TCPListenSocketTest, ServerSendMultiple) {
  tester_->TestServerSendMultiple();
}

}  // namespace net
