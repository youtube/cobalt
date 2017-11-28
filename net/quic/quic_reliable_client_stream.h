// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: This code is not shared between Google and Chrome.

#ifndef NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
#define NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_

#include "net/base/ip_endpoint.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"
#include "net/http/http_stream.h"
#include "net/quic/reliable_quic_stream.h"

namespace net {

class QuicClientSession;

// A client-initiated ReliableQuicStream.  Instances of this class
// are owned by the QuicClientSession which created them.
class NET_EXPORT_PRIVATE QuicReliableClientStream : public ReliableQuicStream {
 public:
  // Delegate handles protocol specific behavior of a quic stream.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() {}

    // Called when stream is ready to send data.
    // Returns network error code. OK when it successfully sent data.
    // ERR_IO_PENDING when performing operation asynchronously.
    virtual int OnSendData() = 0;

    // Called when data has been sent. |status| indicates network error
    // or number of bytes that has been sent. On return, |eof| is set to true
    // if no more data is available to send.
    // Returns network error code. OK when it successfully sent data.
    virtual int OnSendDataComplete(int status, bool* eof) = 0;

    // Called when data is received.
    // Returns network error code. OK when it successfully receives data.
    virtual int OnDataReceived(const char* data, int length) = 0;

    // Called when the stream is closed by the peer.
    virtual void OnClose(QuicErrorCode error) = 0;

   protected:
    virtual ~Delegate() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

  QuicReliableClientStream(QuicStreamId id,
                           QuicSession* session);

  virtual ~QuicReliableClientStream();

  // ReliableQuicStream
  virtual uint32 ProcessData(const char* data, uint32 data_len) override;
  virtual void TerminateFromPeer(bool half_close) override;
  using ReliableQuicStream::WriteData;

  // Set new |delegate|. |delegate| must not be NULL.
  // If this stream has already received data, OnDataReceived() will be
  // called on the delegate.
  void SetDelegate(Delegate* delegate);
  Delegate* GetDelegate() { return delegate_; }

 private:
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(QuicReliableClientStream);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_RELIABLE_CLIENT_STREAM_H_
