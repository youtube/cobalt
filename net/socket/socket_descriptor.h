// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_DESCRIPTOR_H_
#define NET_SOCKET_SOCKET_DESCRIPTOR_H_

#include "build/build_config.h"
#include "net/base/net_export.h"

#if defined(OS_WIN)
#include <winsock2.h>
#endif  // OS_WIN
#if defined(STARBOARD)
#include "starboard/common/socket.h"
#include "starboard/types.h"
#endif

namespace net {

#if defined(OS_WIN)
typedef SOCKET SocketDescriptor;
const SocketDescriptor kInvalidSocket = INVALID_SOCKET;
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
typedef int SocketDescriptor;
const SocketDescriptor kInvalidSocket = -1;
#elif defined(STARBOARD)
typedef SbSocket SocketDescriptor;
#endif

#if !defined(STARBOARD)
// Creates  socket. See WSASocket/socket documentation of parameters.
SocketDescriptor NET_EXPORT CreatePlatformSocket(int family,
                                                 int type,
                                                 int protocol);
#endif

}  // namespace net

#endif  // NET_SOCKET_SOCKET_DESCRIPTOR_H_
