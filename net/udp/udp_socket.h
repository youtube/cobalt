// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_UDP_UDP_SOCKET_H_
#define NET_UDP_UDP_SOCKET_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include "net/udp/udp_socket_win.h"
#elif defined(OS_POSIX)
#include "net/udp/udp_socket_libevent.h"
#endif

namespace net {

// A client socket that uses UDP as the transport layer.
#if defined(OS_WIN)
typedef UDPSocketWin UDPSocket;
#elif defined(OS_POSIX)
typedef UDPSocketLibevent UDPSocket;
#endif

}  // namespace net

#endif  // NET_UDP_UDP_SOCKET_H_
