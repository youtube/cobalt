// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/sys_addrinfo.h"
#include "net/base/test_completion_callback.h"
#include "net/socket_stream/socket_stream.h"
#include "net/websockets/websocket_job.h"
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

  static void MockSocketStreamConnect(
      SocketStream* socket, struct addrinfo* head) {
    socket->CopyAddrInfo(head);
    // In SocketStream::Connect(), it adds reference to socket, which is
    // balanced with SocketStream::Finish() that is finally called from
    // SocketStream::Close() or SocketStream::DetachDelegate(), when
    // next_state_ is not STATE_NONE.
    // If next_state_ is STATE_NONE, SocketStream::Close() or
    // SocketStream::DetachDelegate() won't call SocketStream::Finish(),
    // so Release() won't be called.  Thus, we don't need socket->AddRef()
    // here.
    DCHECK_EQ(socket->next_state_, SocketStream::STATE_NONE);
  }
};

TEST_F(WebSocketThrottleTest, Throttle) {
  DummySocketStreamDelegate delegate;

  // For host1: 1.2.3.4, 1.2.3.5, 1.2.3.6
  struct addrinfo* addr = AddAddr(1, 2, 3, 4, NULL);
  addr = AddAddr(1, 2, 3, 5, addr);
  addr = AddAddr(1, 2, 3, 6, addr);
  scoped_refptr<WebSocketJob> w1(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s1(
      new SocketStream(GURL("ws://host1/"), w1.get()));
  w1->InitSocketStream(s1.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s1, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket1";
  TestCompletionCallback callback_s1;
  // Trying to open connection to host1 will start without wait.
  EXPECT_EQ(OK, w1->OnStartOpenConnection(s1, &callback_s1));

  // Now connecting to host1, so waiting queue looks like
  // Address | head -> tail
  // 1.2.3.4 | w1
  // 1.2.3.5 | w1
  // 1.2.3.6 | w1

  // For host2: 1.2.3.4
  addr = AddAddr(1, 2, 3, 4, NULL);
  scoped_refptr<WebSocketJob> w2(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s2(
      new SocketStream(GURL("ws://host2/"), w2.get()));
  w2->InitSocketStream(s2.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s2, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket2";
  TestCompletionCallback callback_s2;
  // Trying to open connection to host2 will wait for w1.
  EXPECT_EQ(ERR_IO_PENDING, w2->OnStartOpenConnection(s2, &callback_s2));
  // Now waiting queue looks like
  // Address | head -> tail
  // 1.2.3.4 | w1 w2
  // 1.2.3.5 | w1
  // 1.2.3.6 | w1

  // For host3: 1.2.3.5
  addr = AddAddr(1, 2, 3, 5, NULL);
  scoped_refptr<WebSocketJob> w3(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s3(
      new SocketStream(GURL("ws://host3/"), w3.get()));
  w3->InitSocketStream(s3.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s3, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket3";
  TestCompletionCallback callback_s3;
  // Trying to open connection to host3 will wait for w1.
  EXPECT_EQ(ERR_IO_PENDING, w3->OnStartOpenConnection(s3, &callback_s3));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1

  // For host4: 1.2.3.4, 1.2.3.6
  addr = AddAddr(1, 2, 3, 4, NULL);
  addr = AddAddr(1, 2, 3, 6, addr);
  scoped_refptr<WebSocketJob> w4(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s4(
      new SocketStream(GURL("ws://host4/"), w4.get()));
  w4->InitSocketStream(s4.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s4, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket4";
  TestCompletionCallback callback_s4;
  // Trying to open connection to host4 will wait for w1, w2.
  EXPECT_EQ(ERR_IO_PENDING, w4->OnStartOpenConnection(s4, &callback_s4));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2    w4
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1       w4

  // For host5: 1.2.3.6
  addr = AddAddr(1, 2, 3, 6, NULL);
  scoped_refptr<WebSocketJob> w5(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s5(
      new SocketStream(GURL("ws://host5/"), w5.get()));
  w5->InitSocketStream(s5.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s5, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket5";
  TestCompletionCallback callback_s5;
  // Trying to open connection to host5 will wait for w1, w4
  EXPECT_EQ(ERR_IO_PENDING, w5->OnStartOpenConnection(s5, &callback_s5));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2    w4
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1       w4 w5

  // For host6: 1.2.3.6
  addr = AddAddr(1, 2, 3, 6, NULL);
  scoped_refptr<WebSocketJob> w6(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s6(
      new SocketStream(GURL("ws://host6/"), w6.get()));
  w6->InitSocketStream(s6.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s6, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket6";
  TestCompletionCallback callback_s6;
  // Trying to open connection to host6 will wait for w1, w4, w5
  EXPECT_EQ(ERR_IO_PENDING, w6->OnStartOpenConnection(s6, &callback_s6));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2    w4
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1       w4 w5 w6

  // Receive partial response on w1, still connecting.
  DVLOG(1) << "socket1 1";
  static const char kHeader[] = "HTTP/1.1 101 WebSocket Protocol\r\n";
  w1->OnReceivedData(s1.get(), kHeader, sizeof(kHeader) - 1);
  EXPECT_FALSE(callback_s2.have_result());
  EXPECT_FALSE(callback_s3.have_result());
  EXPECT_FALSE(callback_s4.have_result());
  EXPECT_FALSE(callback_s5.have_result());
  EXPECT_FALSE(callback_s6.have_result());

  // Receive rest of handshake response on w1.
  DVLOG(1) << "socket1 2";
  static const char kHeader2[] =
      "Upgrade: WebSocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Origin: http://www.google.com\r\n"
      "Sec-WebSocket-Location: ws://websocket.chromium.org\r\n"
      "\r\n"
      "8jKS'y:G*Co,Wxa-";
  w1->OnReceivedData(s1.get(), kHeader2, sizeof(kHeader2) - 1);
  MessageLoopForIO::current()->RunAllPending();
  // Now, w1 is open.
  EXPECT_EQ(WebSocketJob::OPEN, w1->state());
  // So, w2 and w3 can start connecting. w4 needs to wait w2 (1.2.3.4)
  EXPECT_TRUE(callback_s2.have_result());
  EXPECT_TRUE(callback_s3.have_result());
  EXPECT_FALSE(callback_s4.have_result());
  // Address | head -> tail
  // 1.2.3.4 |    w2    w4
  // 1.2.3.5 |       w3
  // 1.2.3.6 |          w4 w5 w6

  // Closing s1 doesn't change waiting queue.
  DVLOG(1) << "socket1 close";
  w1->OnClose(s1.get());
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_FALSE(callback_s4.have_result());
  s1->DetachDelegate();
  // Address | head -> tail
  // 1.2.3.4 |    w2    w4
  // 1.2.3.5 |       w3
  // 1.2.3.6 |          w4 w5 w6

  // w5 can close while waiting in queue.
  DVLOG(1) << "socket5 close";
  // w5 close() closes SocketStream that change state to STATE_CLOSE, calls
  // DoLoop(), so OnClose() callback will be called.
  w5->OnClose(s5.get());
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_FALSE(callback_s4.have_result());
  // Address | head -> tail
  // 1.2.3.4 |    w2    w4
  // 1.2.3.5 |       w3
  // 1.2.3.6 |          w4 w6
  s5->DetachDelegate();

  // w6 close abnormally (e.g. renderer finishes) while waiting in queue.
  DVLOG(1) << "socket6 close abnormally";
  w6->DetachDelegate();
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_FALSE(callback_s4.have_result());
  // Address | head -> tail
  // 1.2.3.4 |    w2    w4
  // 1.2.3.5 |       w3
  // 1.2.3.6 |          w4

  // Closing s2 kicks w4 to start connecting.
  DVLOG(1) << "socket2 close";
  w2->OnClose(s2.get());
  MessageLoopForIO::current()->RunAllPending();
  EXPECT_TRUE(callback_s4.have_result());
  // Address | head -> tail
  // 1.2.3.4 |          w4
  // 1.2.3.5 |       w3
  // 1.2.3.6 |          w4
  s2->DetachDelegate();

  DVLOG(1) << "socket3 close";
  w3->OnClose(s3.get());
  MessageLoopForIO::current()->RunAllPending();
  s3->DetachDelegate();
  w4->OnClose(s4.get());
  s4->DetachDelegate();
  DVLOG(1) << "Done";
  MessageLoopForIO::current()->RunAllPending();
}

TEST_F(WebSocketThrottleTest, NoThrottleForDuplicateAddress) {
  DummySocketStreamDelegate delegate;

  // For localhost: 127.0.0.1, 127.0.0.1
  struct addrinfo* addr = AddAddr(127, 0, 0, 1, NULL);
  addr = AddAddr(127, 0, 0, 1, addr);
  scoped_refptr<WebSocketJob> w1(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s1(
      new SocketStream(GURL("ws://localhost/"), w1.get()));
  w1->InitSocketStream(s1.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s1, addr);
  DeleteAddrInfo(addr);

  DVLOG(1) << "socket1";
  TestCompletionCallback callback_s1;
  // Trying to open connection to localhost will start without wait.
  EXPECT_EQ(OK, w1->OnStartOpenConnection(s1, &callback_s1));

  DVLOG(1) << "socket1 close";
  w1->OnClose(s1.get());
  s1->DetachDelegate();
  DVLOG(1) << "Done";
  MessageLoopForIO::current()->RunAllPending();
}

}
