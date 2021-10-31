// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H
#define NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H

#include "net/socket/udp_socket.h"

namespace net {

class IPEndPoint;

class UdpSocketFactory {
 public:
  std::unique_ptr<UDPSocket> CreateAndBind(const IPEndPoint& address);
  virtual ~UdpSocketFactory() {}

 protected:
  virtual void SetupSocketAfterBind(UDPSocket* sock) {}
};

class DialUdpSocketFactory : public UdpSocketFactory {
 public:
  virtual ~DialUdpSocketFactory() {}

 protected:
  // UdpSocketFactory implementation
  virtual void SetupSocketAfterBind(UDPSocket* sock) override;
};

}  // namespace net

#endif  // NET_DIAL_DIAL_UDP_SOCKET_FACTORY_H
