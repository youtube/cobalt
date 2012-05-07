// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/websockets/websocket_throttle.h"

#include <string>

#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/address_list.h"
#include "net/base/test_completion_callback.h"
#include "net/socket_stream/socket_stream.h"
#include "net/url_request/url_request_test_util.h"
#include "net/websockets/websocket_job.h"
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
  IPEndPoint MakeAddr(int a1, int a2, int a3, int a4) {
    IPAddressNumber ip;
    ip.push_back(a1);
    ip.push_back(a2);
    ip.push_back(a3);
    ip.push_back(a4);
    return IPEndPoint(ip, 0);
  }

  static void MockSocketStreamConnect(
      SocketStream* socket, const AddressList& list) {
    socket->set_addresses(list);
    // TODO(toyoshim): We should introduce additional tests on cases via proxy.
    socket->proxy_info_.UseDirect();
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
  scoped_refptr<URLRequestContext> context(new TestURLRequestContext);
  DummySocketStreamDelegate delegate;
  // TODO(toyoshim): We need to consider both spdy-enabled and spdy-disabled
  // configuration.
  WebSocketJob::set_websocket_over_spdy_enabled(true);

  // For host1: 1.2.3.4, 1.2.3.5, 1.2.3.6
  AddressList addr;
  addr.push_back(MakeAddr(1, 2, 3, 4));
  addr.push_back(MakeAddr(1, 2, 3, 5));
  addr.push_back(MakeAddr(1, 2, 3, 6));
  scoped_refptr<WebSocketJob> w1(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s1(
      new SocketStream(GURL("ws://host1/"), w1.get()));
  s1->set_context(context.get());
  w1->InitSocketStream(s1.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s1, addr);

  DVLOG(1) << "socket1";
  TestCompletionCallback callback_s1;
  // Trying to open connection to host1 will start without wait.
  EXPECT_EQ(OK, w1->OnStartOpenConnection(s1, callback_s1.callback()));

  // Now connecting to host1, so waiting queue looks like
  // Address | head -> tail
  // 1.2.3.4 | w1
  // 1.2.3.5 | w1
  // 1.2.3.6 | w1

  // For host2: 1.2.3.4
  addr.clear();
  addr.push_back(MakeAddr(1, 2, 3, 4));
  scoped_refptr<WebSocketJob> w2(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s2(
      new SocketStream(GURL("ws://host2/"), w2.get()));
  s2->set_context(context.get());
  w2->InitSocketStream(s2.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s2, addr);

  DVLOG(1) << "socket2";
  TestCompletionCallback callback_s2;
  // Trying to open connection to host2 will wait for w1.
  EXPECT_EQ(ERR_IO_PENDING,
            w2->OnStartOpenConnection(s2, callback_s2.callback()));
  // Now waiting queue looks like
  // Address | head -> tail
  // 1.2.3.4 | w1 w2
  // 1.2.3.5 | w1
  // 1.2.3.6 | w1

  // For host3: 1.2.3.5
  addr.clear();
  addr.push_back(MakeAddr(1, 2, 3, 5));
  scoped_refptr<WebSocketJob> w3(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s3(
      new SocketStream(GURL("ws://host3/"), w3.get()));
  s3->set_context(context.get());
  w3->InitSocketStream(s3.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s3, addr);

  DVLOG(1) << "socket3";
  TestCompletionCallback callback_s3;
  // Trying to open connection to host3 will wait for w1.
  EXPECT_EQ(ERR_IO_PENDING,
            w3->OnStartOpenConnection(s3, callback_s3.callback()));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1

  // For host4: 1.2.3.4, 1.2.3.6
  addr.clear();
  addr.push_back(MakeAddr(1, 2, 3, 4));
  addr.push_back(MakeAddr(1, 2, 3, 6));
  scoped_refptr<WebSocketJob> w4(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s4(
      new SocketStream(GURL("ws://host4/"), w4.get()));
  s4->set_context(context.get());
  w4->InitSocketStream(s4.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s4, addr);

  DVLOG(1) << "socket4";
  TestCompletionCallback callback_s4;
  // Trying to open connection to host4 will wait for w1, w2.
  EXPECT_EQ(ERR_IO_PENDING,
            w4->OnStartOpenConnection(s4, callback_s4.callback()));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2    w4
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1       w4

  // For host5: 1.2.3.6
  addr.clear();
  addr.push_back(MakeAddr(1, 2, 3, 6));
  scoped_refptr<WebSocketJob> w5(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s5(
      new SocketStream(GURL("ws://host5/"), w5.get()));
  s5->set_context(context.get());
  w5->InitSocketStream(s5.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s5, addr);

  DVLOG(1) << "socket5";
  TestCompletionCallback callback_s5;
  // Trying to open connection to host5 will wait for w1, w4
  EXPECT_EQ(ERR_IO_PENDING,
            w5->OnStartOpenConnection(s5, callback_s5.callback()));
  // Address | head -> tail
  // 1.2.3.4 | w1 w2    w4
  // 1.2.3.5 | w1    w3
  // 1.2.3.6 | w1       w4 w5

  // For host6: 1.2.3.6
  addr.clear();
  addr.push_back(MakeAddr(1, 2, 3, 6));
  scoped_refptr<WebSocketJob> w6(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s6(
      new SocketStream(GURL("ws://host6/"), w6.get()));
  s6->set_context(context.get());
  w6->InitSocketStream(s6.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s6, addr);

  DVLOG(1) << "socket6";
  TestCompletionCallback callback_s6;
  // Trying to open connection to host6 will wait for w1, w4, w5
  EXPECT_EQ(ERR_IO_PENDING,
            w6->OnStartOpenConnection(s6, callback_s6.callback()));
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
  scoped_refptr<URLRequestContext> context(new TestURLRequestContext);
  DummySocketStreamDelegate delegate;
  WebSocketJob::set_websocket_over_spdy_enabled(true);

  // For localhost: 127.0.0.1, 127.0.0.1
  AddressList addr;
  addr.push_back(MakeAddr(127, 0, 0, 1));
  addr.push_back(MakeAddr(127, 0, 0, 1));
  scoped_refptr<WebSocketJob> w1(new WebSocketJob(&delegate));
  scoped_refptr<SocketStream> s1(
      new SocketStream(GURL("ws://localhost/"), w1.get()));
  s1->set_context(context.get());
  w1->InitSocketStream(s1.get());
  WebSocketThrottleTest::MockSocketStreamConnect(s1, addr);

  DVLOG(1) << "socket1";
  TestCompletionCallback callback_s1;
  // Trying to open connection to localhost will start without wait.
  EXPECT_EQ(OK, w1->OnStartOpenConnection(s1, callback_s1.callback()));

  DVLOG(1) << "socket1 close";
  w1->OnClose(s1.get());
  s1->DetachDelegate();
  DVLOG(1) << "Done";
  MessageLoopForIO::current()->RunAllPending();
}

}
