// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H
#define NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H

#include "net/udp/udp_listen_socket.h"

namespace net {

class IPEndPoint;

class UdpSocketFactory {
 public:
  scoped_refptr<UDPListenSocket> CreateAndBind(
      const IPEndPoint& address, UDPListenSocket::Delegate* del);
  virtual ~UdpSocketFactory() {}

 protected:
  virtual void SetupSocketAfterCreate(SocketDescriptor) { }
  virtual void SetupSocketAfterBind(SocketDescriptor) { }
};

class DialUdpSocketFactory : public UdpSocketFactory {
 public:
  virtual ~DialUdpSocketFactory() { }
 protected:
  // UdpSocketFactory implementation
  virtual void SetupSocketAfterCreate(SocketDescriptor) override;
  virtual void SetupSocketAfterBind(SocketDescriptor) override;
};

} // namespace net

#endif // NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H

