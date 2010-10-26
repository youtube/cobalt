// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_TCP_CLIENT_SOCKET_H_
#define NET_SOCKET_TCP_CLIENT_SOCKET_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include "net/socket/tcp_client_socket_win.h"
#elif defined(OS_POSIX)
#include "net/socket/tcp_client_socket_libevent.h"
#endif

namespace net {

// A client socket that uses TCP as the transport layer.
#if defined(OS_WIN)
typedef TCPClientSocketWin TCPClientSocket;
#elif defined(OS_POSIX)
typedef TCPClientSocketLibevent TCPClientSocket;
#endif

// Enable/disable experimental TCP FastOpen option.
// Not thread safe.  Must be called during initialization/startup only.
void set_tcp_fastopen_enabled(bool value);

// Check if the TCP FastOpen option is enabled.
bool is_tcp_fastopen_enabled();

}  // namespace net

#endif  // NET_SOCKET_TCP_CLIENT_SOCKET_H_
