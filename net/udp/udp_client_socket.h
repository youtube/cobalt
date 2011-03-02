// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_UDP_CLIENT_SOCKET_H_
#define NET_SOCKET_UDP_CLIENT_SOCKET_H_
#pragma once

#include "net/base/net_log.h"
#include "net/udp/datagram_client_socket.h"
#include "net/udp/udp_socket.h"

namespace net {

class BoundNetLog;

// A client socket that uses UDP as the transport layer.
class UDPClientSocket : public DatagramClientSocket {
 public:
  UDPClientSocket(net::NetLog* net_log,
                  const net::NetLog::Source& source);
  virtual ~UDPClientSocket();

  // Implement DatagramClientSocket:
  virtual int Connect(const AddressList& address);
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual void Close();
  virtual int GetPeerAddress(AddressList* address) const;
  virtual int GetLocalAddress(AddressList* address) const;
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  UDPSocket socket_;
  DISALLOW_COPY_AND_ASSIGN(UDPClientSocket);
};

}  // namespace net

#endif  // NET_SOCKET_UDP_CLIENT_SOCKET_H_
