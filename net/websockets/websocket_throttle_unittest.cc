// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_throttle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

class DummySocketStreamDelegate : public net::SocketStream::Delegate {
 public:
  DummySocketStreamDelegate() {}
  virtual ~DummySocketStreamDelegate() {}
  virtual void OnConnected(
      net::SocketStream* socket, int max_pending_send_allowed) {}
  virtual void OnSentData(net::SocketStream* socket, int amount_sent) {}
  virtual void OnReceivedData(net::SocketStream* socket,
                              const char* data, int len) {}
  virtual void OnClose(net::SocketStream* socket) {}
};

namespace net {

class WebSocketThrottleTest : public PlatformTest {
 protected:
  struct addrinfo *AddAddr(int a1, int a2, int a3, int a4,
                           struct addrinfo* next) {
    struct addrinfo* addrinfo = new struct addrinfo;
    memset(addrinfo, 0, sizeof(struct addrinfo));
    addrinfo->ai_family = AF_INET;
    int addrlen = sizeof(struct sockaddr_in);
    addrinfo->ai_addrlen = addrlen;
    addrinfo->ai_addr = reinterpret_cast<sockaddr*>(new char[addrlen]);
    memset(addrinfo->ai_addr, 0, sizeof(addrlen));
    struct sockaddr_in* addr =
        reinterpret_cast<sockaddr_in*>(addrinfo->ai_addr);
    int addrint = ((a1 & 0xff) << 24) |
        ((a2 & 0xff) << 16) |
        ((a3 & 0xff) <<  8) |
        ((a4 & 0xff));
    memcpy(&addr->sin_addr, &addrint, sizeof(int));
    addrinfo->ai_next = next;
    return addrinfo;
  }
  void DeleteAddrInfo(struct addrinfo* head) {
    if (!head)
      return;
    struct addrinfo* next;
    for (struct addrinfo* a = head; a != NULL; a = next) {
      next = a->ai_next;
      delete [] a->ai_addr;
      delete a;
    }
  }

  static void SetAddressList(SocketStream* socket, struct addrinfo* head) {
    socket->CopyAddrInfo(head);
  }
};

TEST_F(WebSocketThrottleTest, Throttle) {
  WebSocketThrottle::Init();
  DummySocketStreamDelegate delegate;

  WebSocketThrottle* throttle = Singleton<WebSocketThrottle>::get();

  EXPECT_EQ(throttle,
            SocketStreamThrottle::GetSocketStreamThrottleForScheme("ws"));
  EXPECT_EQ(throttle,
            SocketStreamThrottle::GetSocketStreamThrottleForScheme("wss"));

  // For host1: 1.2.3.4, 1.2.3.5, 1.2.3.6
  struct addrinfo* addr = AddAddr(1, 2, 3, 4, NULL);
  addr = AddAddr(1, 2, 3, 5, addr);
  addr = AddAddr(1, 2, 3, 6, addr);
  scoped_refptr<SocketStream> s1 =
      new SocketStream(GURL("ws://host1/"), &delegate);
  WebSocketThrottleTest::SetAddressList(s1, addr);
  DeleteAddrInfo(addr);

  TestCompletionCallback callback_s1;
  EXPECT_EQ(OK, throttle->OnStartOpenConnection(s1, &callback_s1));

  // For host2: 1.2.3.4
  addr = AddAddr(1, 2, 3, 4, NULL);
  scoped_refptr<SocketStream> s2 =
      new SocketStream(GURL("ws://host2/"), &delegate);
  WebSocketThrottleTest::SetAddressList(s2, addr);
  DeleteAddrInfo(addr);

  TestCompletionCallback callback_s2;
  EXPECT_EQ(ERR_IO_PENDING, throttle->OnStartOpenConnection(s2, &callback_s2));

  // For host3: 1.2.3.5
  addr = AddAddr(1, 2, 3, 5, NULL);
  scoped_refptr<SocketStream> s3 =
      new SocketStream(GURL("ws://host3/"), &delegate);
  WebSocketThrottleTest::SetAddressList(s3, addr);
  DeleteAddrInfo(addr);

  TestCompletionCallback callback_s3;
  EXPECT_EQ(ERR_IO_PENDING, throttle->OnStartOpenConnection(s3, &callback_s3));

  // For host4: 1.2.3.4, 1.2.3.6
  addr = AddAddr(1, 2, 3, 4, NULL);
  addr = AddAddr(1, 2, 3, 6, addr);
  scoped_refptr<SocketStream> s4 =
      new SocketStream(GURL("ws://host4/"), &delegate);
  WebSocketThrottleTest::SetAddressList(s4, addr);
  DeleteAddrInfo(addr);

  TestCompletionCallback callback_s4;
  EXPECT_EQ(ERR_IO_PENDING, throttle->OnStartOpenConnection(s4, &callback_s4));

  static const char kHeader[] = "HTTP/1.1 101 Web Socket Protocol\r\n";
  EXPECT_EQ(OK,
            throttle->OnRead(s1.get(), kHeader, sizeof(kHeader) - 1, NULL));
  EXPECT_FALSE(callback_s2.have_result());
  EXPECT_FALSE(callback_s3.have_result());
  EXPECT_FALSE(callback_s4.have_result());

  static const char kHeader2[] =
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "WebSocket-Origin: http://www.google.com\r\n"
      "WebSocket-Location: ws://websocket.chromium.org\r\n"
      "\r\n";
  EXPECT_EQ(OK,
            throttle->OnRead(s1.get(), kHeader2, sizeof(kHeader2) - 1, NULL));
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_TRUE(callback_s2.have_result());
  EXPECT_TRUE(callback_s3.have_result());
  EXPECT_FALSE(callback_s4.have_result());

  throttle->OnClose(s1.get());
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_FALSE(callback_s4.have_result());
  s1->DetachDelegate();

  throttle->OnClose(s2.get());
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_TRUE(callback_s4.have_result());
  s2->DetachDelegate();

  throttle->OnClose(s3.get());
  MessageLoopForIO::current()->RunAllPending();
  s3->DetachDelegate();
  throttle->OnClose(s4.get());
  s4->DetachDelegate();
}

}
