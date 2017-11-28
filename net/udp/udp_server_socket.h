// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_SERVER_SOCKET_H_
#define NET_SOCKET_UDP_SERVER_SOCKET_H_

#include "net/base/completion_callback.h"
#include "net/udp/datagram_server_socket.h"
#include "net/udp/udp_socket.h"

namespace net {

class IPEndPoint;
class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class NET_EXPORT UDPServerSocket : public DatagramServerSocket {
 public:
  UDPServerSocket(net::NetLog* net_log,
                  const net::NetLog::Source& source);
  virtual ~UDPServerSocket();

  // Implement DatagramServerSocket:
  virtual int Listen(const IPEndPoint& address) override;
  virtual int RecvFrom(IOBuffer* buf,
                       int buf_len,
                       IPEndPoint* address,
                       const CompletionCallback& callback) override;
  virtual int SendTo(IOBuffer* buf,
                     int buf_len,
                     const IPEndPoint& address,
                     const CompletionCallback& callback) override;
  virtual bool SetReceiveBufferSize(int32 size) override;
  virtual bool SetSendBufferSize(int32 size) override;
  virtual void Close() override;
  virtual int GetPeerAddress(IPEndPoint* address) const override;
  virtual int GetLocalAddress(IPEndPoint* address) const override;
  virtual const BoundNetLog& NetLog() const override;
  virtual void AllowAddressReuse() override;
  virtual void AllowBroadcast() override;

 private:
  UDPSocket socket_;
  DISALLOW_COPY_AND_ASSIGN(UDPServerSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_SERVER_SOCKET_H_
