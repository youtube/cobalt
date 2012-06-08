// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_BUFFERED_WRITE_STREAM_SOCKET_H_
#define NET_SOCKET_BUFFERED_WRITE_STREAM_SOCKET_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"

namespace base {
class TimeDelta;
}

namespace net {

class AddressList;
class GrowableIOBuffer;
class IPEndPoint;

// A StreamSocket decorator. All functions are passed through to the wrapped
// socket, except for Write().
//
// Writes are buffered locally so that multiple Write()s to this class are
// issued as only one Write() to the wrapped socket. This is useful to force
// multiple requests to be issued in a single packet, as is needed to trigger
// edge cases in HTTP pipelining.
//
// Note that the Write() always returns synchronously. It will either buffer the
// entire input or return the most recently reported error.
//
// There are no bounds on the local buffer size. Use carefully.
class NET_EXPORT_PRIVATE BufferedWriteStreamSocket : public StreamSocket {
 public:
  BufferedWriteStreamSocket(StreamSocket* socket_to_wrap);
  virtual ~BufferedWriteStreamSocket();

  // Socket interface
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // StreamSocket interface
  virtual int Connect(const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE;

 private:
  void DoDelayedWrite();
  void OnIOComplete(int result);

  scoped_ptr<StreamSocket> wrapped_socket_;
  scoped_refptr<GrowableIOBuffer> io_buffer_;
  scoped_refptr<GrowableIOBuffer> backup_buffer_;
  base::WeakPtrFactory<BufferedWriteStreamSocket> weak_factory_;
  bool callback_pending_;
  bool wrapped_write_in_progress_;
  int error_;
};

}  // namespace net

#endif  // NET_SOCKET_STREAM_SOCKET_H_
