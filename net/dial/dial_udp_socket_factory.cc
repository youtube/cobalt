// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_udp_socket_factory.h"

#if !defined(STARBOARD)
#include <arpa/inet.h>
#endif

#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_string_util.h"

#if defined(STARBOARD)
#include "starboard/common/socket.h"
#include "starboard/types.h"
#endif

namespace net {

std::unique_ptr<UDPSocket> UdpSocketFactory::CreateAndBind(
    const IPEndPoint& address) {
  std::unique_ptr<UDPSocket> sock(
      new UDPSocket(DatagramSocket::DEFAULT_BIND, nullptr, NetLogSource()));
  if (sock->Open(address.GetFamily()) != net::OK) {
    return NULL;
  }

  int set_socket_reuse_success = sock->AllowAddressReuse();
  if (set_socket_reuse_success != OK) {
    LOG(WARNING) << "AllowAddressReuse failed on DIAL UDP socket with code "
                 << set_socket_reuse_success;
  }

  if (sock->Bind(address) != net::OK) {
    LOG(WARNING) << "DIAL UDP socket failed to bind to address";
    sock->Close();
    return NULL;
  }

  SetupSocketAfterBind(sock.get());

  return sock;
}
// Enable Multicast and join group if multicast is enabled.
void DialUdpSocketFactory::SetupSocketAfterBind(UDPSocket* sock) {
  net::IPAddress address(239, 255, 255, 250);
  if (sock->JoinGroup(address) != net::OK) {
    LOG(WARNING) << "JoinGroup failed on DIAL UDP socket.";
  }
}

}  // namespace net
