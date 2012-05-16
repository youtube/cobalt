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
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#include "net/base/winsock_init.h"

using std::string;

namespace net {

// static
scoped_refptr<TCPListenSocket> TCPListenSocket::CreateAndListen(
    const string& ip, int port, StreamListenSocket::Delegate* del) {
  SOCKET s = CreateAndBind(ip, port);
  if (s == kInvalidSocket)
    return NULL;
  scoped_refptr<TCPListenSocket> sock(new TCPListenSocket(s, del));
  sock->Listen();
  return sock;
}

TCPListenSocket::TCPListenSocket(SOCKET s, StreamListenSocket::Delegate* del)
    : StreamListenSocket(s, del) {
}

TCPListenSocket::~TCPListenSocket() {}

SOCKET TCPListenSocket::CreateAndBind(const string& ip, int port) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s != kInvalidSocket) {
#if defined(OS_POSIX)
    // Allow rapid reuse.
    static const int kOn = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &kOn, sizeof(kOn));
#endif
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = base::HostToNet16(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
#if defined(OS_WIN)
      closesocket(s);
#elif defined(OS_POSIX)
      close(s);
#endif
      LOG(ERROR) << "Could not bind socket to " << ip << ":" << port;
      s = kInvalidSocket;
    }
  }
  return s;
}

void TCPListenSocket::Accept() {
  SOCKET conn = AcceptSocket();
  if (conn == kInvalidSocket)
    return;
  scoped_refptr<TCPListenSocket> sock(
      new TCPListenSocket(conn, socket_delegate_));
  // It's up to the delegate to AddRef if it wants to keep it around.
#if defined(OS_POSIX)
  sock->WatchSocket(WAITING_READ);
#endif
  socket_delegate_->DidAccept(this, sock);
}

}  // namespace net
