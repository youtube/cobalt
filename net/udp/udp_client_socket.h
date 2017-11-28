// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_CLIENT_SOCKET_H_
#define NET_SOCKET_UDP_CLIENT_SOCKET_H_

#include "net/base/net_log.h"
#include "net/base/rand_callback.h"
#include "net/udp/datagram_client_socket.h"
#include "net/udp/udp_socket.h"

namespace net {

class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class NET_EXPORT_PRIVATE UDPClientSocket : public DatagramClientSocket {
 public:
  UDPClientSocket(DatagramSocket::BindType bind_type,
                  const RandIntCallback& rand_int_cb,
                  net::NetLog* net_log,
                  const net::NetLog::Source& source);
  virtual ~UDPClientSocket();

  // DatagramClientSocket implementation.
  virtual int Connect(const IPEndPoint& address) override;
  virtual int Read(IOBuffer* buf, int buf_len,
                   const CompletionCallback& callback) override;
  virtual int Write(IOBuffer* buf, int buf_len,
                    const CompletionCallback& callback) override;
  virtual void Close() override;
  virtual int GetPeerAddress(IPEndPoint* address) const override;
  virtual int GetLocalAddress(IPEndPoint* address) const override;
  virtual bool SetReceiveBufferSize(int32 size) override;
  virtual bool SetSendBufferSize(int32 size) override;
  virtual const BoundNetLog& NetLog() const override;

 private:
  UDPSocket socket_;
  DISALLOW_COPY_AND_ASSIGN(UDPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_CLIENT_SOCKET_H_
