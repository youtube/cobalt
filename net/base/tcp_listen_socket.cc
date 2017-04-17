// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/tcp_listen_socket.h"

#if defined(OS_WIN)
// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net/base/net_errors.h"
#endif

#include "base/logging.h"
#include "base/optional.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_util.h"

using std::string;

namespace net {

#if defined(OS_STARBOARD)
namespace {
base::optional<IPEndPoint> ToIPEndPoint(const string& ip, int port) {
  if (ip.empty()) {
    return IPEndPoint::GetForAllInterfaces(port);
  }

  IPAddressNumber ip_number;
  if (!ParseIPLiteralToNumber(ip, &ip_number)) {
    return base::nullopt;
  }

  return IPEndPoint(ip_number, port);
}
}  // namespace
#endif  // defined(OS_STARBOARD)

// static
scoped_refptr<TCPListenSocket> TCPListenSocket::CreateAndListen(
    const string& ip, int port, StreamListenSocket::Delegate* del) {
  SocketDescriptor s = CreateAndBind(ip, port);
  if (s == kInvalidSocket)
    return NULL;
  scoped_refptr<TCPListenSocket> sock(new TCPListenSocket(s, del));
  sock->Listen();
  return sock;
}

TCPListenSocket::TCPListenSocket(SocketDescriptor s,
                                 StreamListenSocket::Delegate* del)
    : StreamListenSocket(s, del) {
}

TCPListenSocket::~TCPListenSocket() {}

SocketDescriptor TCPListenSocket::CreateAndBind(const string& ip, int port) {
#if defined(OS_STARBOARD)
  base::optional<IPEndPoint> endpoint = ToIPEndPoint(ip, port);
  if (!endpoint) {
    return kSbSocketInvalid;
  }
  SbSocketAddressType socket_address_type;
  switch (endpoint->GetFamily()) {
    case ADDRESS_FAMILY_IPV4:
      socket_address_type = kSbSocketAddressTypeIpv4;
      break;
    case ADDRESS_FAMILY_IPV6:
      socket_address_type = kSbSocketAddressTypeIpv6;
      break;
    default:
      return kSbSocketInvalid;
  }

  SocketDescriptor socket =
      SbSocketCreate(socket_address_type, kSbSocketProtocolTcp);
  if (SbSocketIsValid(socket)) {
    SbSocketSetReuseAddress(socket, true);

    SbSocketAddress sb_address;
    bool groovy = endpoint->ToSbSocketAddress(&sb_address);
    if (groovy) {
      groovy &= (SbSocketBind(socket, &sb_address) == kSbSocketOk);
    }

    if (!groovy) {
      SbSocketDestroy(socket);
      return kSbSocketInvalid;
    }
  }

  return socket;
#else  // defined(OS_STARBOARD)
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  SocketDescriptor s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s != kInvalidSocket) {
    SetNonBlocking(s);
#if defined(OS_POSIX)
    // Allow rapid reuse.
    static const int kOn = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &kOn, sizeof(kOn));
#endif  // defined(OS_POSIX)
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = base::HostToNet16(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
#if defined(OS_WIN)
      closesocket(s);
#elif defined(__LB_SHELL__)
      lb_close_socket(s);
#elif defined(OS_POSIX)
      close(s);
#endif
      LOG(ERROR) << "Could not bind socket to " << ip << ":" << port;
      s = kInvalidSocket;
    }
  }
  return s;
#endif  // defined(OS_STARBOARD)
}

SocketDescriptor TCPListenSocket::CreateAndBindAnyPort(const string& ip,
                                                       int* port) {
  SocketDescriptor s = CreateAndBind(ip, 0);
  if (s == kInvalidSocket)
    return kInvalidSocket;

#if defined(OS_STARBOARD)
  SbSocketAddress address;
  if (!SbSocketGetLocalAddress(s, &address)) {
    SbSocketDestroy(s);
    return kSbSocketInvalid;
  }

  *port = address.port;
#else  // defined(OS_STARBOARD)
  sockaddr_in addr;
  socklen_t addr_size = sizeof(addr);
  bool failed = getsockname(s, reinterpret_cast<struct sockaddr*>(&addr),
                            &addr_size) != 0;
  if (addr_size != sizeof(addr))
    failed = true;
  if (failed) {
    LOG(ERROR) << "Could not determine bound port, getsockname() failed";
#if defined(OS_WIN)
    closesocket(s);
#elif defined(__LB_SHELL__)
    lb_close_socket(s);
#elif defined(OS_POSIX)
    close(s);
#endif
    return kInvalidSocket;
  }
  *port = base::NetToHost16(addr.sin_port);
#endif  // defined(OS_STARBOARD)
  return s;
}

void TCPListenSocket::Accept() {
  SocketDescriptor conn = AcceptSocket();
  if (conn == kInvalidSocket)
    return;
  scoped_refptr<TCPListenSocket> sock(
      new TCPListenSocket(conn, socket_delegate_));
  // It's up to the delegate to AddRef if it wants to keep it around.
#if defined(OS_POSIX) || defined(OS_STARBOARD)
  sock->WatchSocket(WAITING_READ);
#endif
  socket_delegate_->DidAccept(this, sock);
}

TCPListenSocketFactory::TCPListenSocketFactory(const string& ip, int port)
    : ip_(ip),
      port_(port) {
}

TCPListenSocketFactory::~TCPListenSocketFactory() {}

#if !defined(COBALT_WIN)
scoped_refptr<StreamListenSocket> TCPListenSocketFactory::CreateAndListen(
    StreamListenSocket::Delegate* delegate) const {
  return TCPListenSocket::CreateAndListen(ip_, port_, delegate);
}
#endif

}  // namespace net
