// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dial/dial_udp_socket_factory.h"

#include <arpa/inet.h>

#include "net/base/net_util.h"
#include "net/base/ip_endpoint.h"
#include "net/udp/udp_listen_socket.h"
#include "lb_network_helpers.h"

namespace net {

namespace { // anonymous

// static methods
#if defined(OS_POSIX)
SocketDescriptor NativeCreateUdpSocket(int family) {
  return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool NativeBind(const SocketDescriptor s, const IPEndPoint& address) {
  SockaddrStorage addr;
  if (!address.ToSockAddr(addr.addr, &addr.addr_len)) {
    LOG(ERROR) << "Failed to resolve IPEndPoint. " << address.ToString();
    return false;
  }

  if (0 != ::bind(s, addr.addr, addr.addr_len)) {
    PLOG(ERROR) << "Failed to bind address: " << address.ToString();
    return false;
  }
  return true;
}
#endif

} // anonymous namespace

scoped_refptr<UDPListenSocket> UdpSocketFactory::CreateAndBind(
    const IPEndPoint& address, UDPListenSocket::Delegate* del) {
 SocketDescriptor s = NativeCreateUdpSocket(address.GetFamily());
 if (s == UDPListenSocket::kInvalidSocket)
   return NULL;

 SetupSocketAfterCreate(s);

 if (!NativeBind(s, address))
   return NULL;

 SetupSocketAfterBind(s);

 scoped_refptr<UDPListenSocket> sock(new UDPListenSocket(s, del));
 return sock;
}

void DialUdpSocketFactory::SetupSocketAfterCreate(SocketDescriptor s) {
#if defined(SO_REUSEADDR)
  int on = 1;
  if (::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    LOG(WARNING) << "Failed to set SO_REUSEADDR on dial UDP socket.";
  }
#else
  NOTIMPLEMENTED() << "Cannot SO_REUSEADDR on this platform.";
#endif
  SetNonBlocking(s);
}

// Enable Multicast and join group if multicast is enabled.
void DialUdpSocketFactory::SetupSocketAfterBind(SocketDescriptor s) {
#if defined(IP_ADD_MEMBERSHIP)
  struct ip_mreq imreq;
  imreq.imr_multiaddr.s_addr = ::inet_addr("239.255.255.250");
#if defined(__LB_WIIU__)
  struct in_addr src_addr;
  ignore_result(LB::Platform::GetLocalIpAddress(&src_addr));
  imreq.imr_interface.s_addr = htonl(src_addr.s_addr);
#else
  imreq.imr_interface.s_addr = INADDR_ANY;
#endif
  if (::setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, &imreq, sizeof(imreq)) < 0) {
    LOG(WARNING) << "Failed to join multicast group on dial UDP socket.";
  }
#else
  NOTIMPLEMENTED() << "No Multicast support on this platform.";
#endif
}


} // namespace net
