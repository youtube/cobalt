// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_WEBSOCKET_STREAM_H_
#define NET_SPDY_SPDY_WEBSOCKET_STREAM_H_

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "net/base/completion_callback.h"
#include "net/base/request_priority.h"
#include "net/spdy/spdy_framer.h"
#include "net/spdy/spdy_header_block.h"
#include "net/spdy/spdy_stream.h"

namespace net {

// The SpdyWebSocketStream is a WebSocket-specific type of stream known to a
// SpdySession. WebSocket's opening handshake is converted to SPDY's
// SYN_STREAM/SYN_REPLY. WebSocket frames are encapsulated as SPDY data frames.
class NET_EXPORT_PRIVATE SpdyWebSocketStream
    : public SpdyStream::Delegate {
 public:
  // Delegate handles asynchronous events.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    // Called when InitializeStream() finishes asynchronously. This delegate is
    // called if InitializeStream() returns ERR_IO_PENDING. |status| indicates
    // network error.
    virtual void OnCreatedSpdyStream(int status) = 0;

    // Called on corresponding to OnSendHeadersComplete() or SPDY's SYN frame
    // has been sent.
    virtual void OnSentSpdyHeaders(int status) = 0;

    // Called on corresponding to OnResponseReceived() or SPDY's SYN_STREAM,
    // SYN_REPLY, or HEADERS frames are received. This callback may be called
    // multiple times as SPDY's delegate does.
    virtual int OnReceivedSpdyResponseHeader(
        const SpdyHeaderBlock& headers,
        int status) = 0;

    // Called when data is sent.
    virtual void OnSentSpdyData(int amount_sent) = 0;

    // Called when data is received.
    virtual void OnReceivedSpdyData(const char* data, int length) = 0;

    // Called when SpdyStream is closed.
    virtual void OnCloseSpdyStream() = 0;

   protected:
    virtual ~Delegate() {}
  };

  SpdyWebSocketStream(SpdySession* spdy_session, Delegate* delegate);
  virtual ~SpdyWebSocketStream();

  // Initializes SPDY stream for the WebSocket.
  // It might create SPDY stream asynchronously.  In this case, this method
  // returns ERR_IO_PENDING and call OnCreatedSpdyStream delegate with result
  // after completion. In other cases, delegate does not be called.
  int InitializeStream(const GURL& url,
                       RequestPriority request_priority,
                       const BoundNetLog& stream_net_log);

  int SendRequest(scoped_ptr<SpdyHeaderBlock> headers);
  int SendData(const char* data, int length);
  void Close();

  // SpdyStream::Delegate
  virtual bool OnSendHeadersComplete(int status) override;
  virtual int OnSendBody() override;
  virtual int OnSendBodyComplete(int status, bool* eof) override;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) override;
  virtual void OnHeadersSent() override;
  virtual int OnDataReceived(const char* data, int length) override;
  virtual void OnDataSent(int length) override;
  virtual void OnClose(int status) override;

 private:
  friend class SpdyWebSocketStreamSpdy2Test;
  friend class SpdyWebSocketStreamSpdy3Test;
  FRIEND_TEST_ALL_PREFIXES(SpdyWebSocketStreamSpdy2Test, Basic);
  FRIEND_TEST_ALL_PREFIXES(SpdyWebSocketStreamSpdy3Test, Basic);

  void OnSpdyStreamCreated(int status);

  scoped_refptr<SpdyStream> stream_;
  scoped_refptr<SpdySession> spdy_session_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(SpdyWebSocketStream);
};

}  // namespace net

#endif  // NET_SPDY_SPDY_WEBSOCKET_STREAM_H_
