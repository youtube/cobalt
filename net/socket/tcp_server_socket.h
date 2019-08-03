// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_SERVER_SOCKET_H_
#define NET_SOCKET_TCP_SERVER_SOCKET_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include "net/socket/tcp_server_socket_win.h"
#elif defined(OS_POSIX)
#include "net/socket/tcp_server_socket_libevent.h"
#endif

namespace net {

#if defined(OS_WIN)
typedef TCPServerSocketWin TCPServerSocket;
#elif defined(OS_POSIX)
typedef TCPServerSocketLibevent TCPServerSocket;
#endif

}  // namespace net

#endif  // NET_SOCKET_TCP_SERVER_SOCKET_H_
