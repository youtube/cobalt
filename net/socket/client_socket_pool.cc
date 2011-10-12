// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool.h"

namespace {

// The maximum duration, in seconds, to keep used idle persistent sockets
// alive.
// TODO(ziadh): Change this timeout after getting histogram data on how long it
// should be.
int g_unused_idle_socket_timeout = 10;

}  // namespace

namespace net {

// static
int ClientSocketPool::unused_idle_socket_timeout() {
  return g_unused_idle_socket_timeout;
}

// static
void ClientSocketPool::set_unused_idle_socket_timeout(int timeout) {
  g_unused_idle_socket_timeout = timeout;
}

ClientSocketPool::ClientSocketPool() {}

ClientSocketPool::~ClientSocketPool() {}

}  // namespace net
