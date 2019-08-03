// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket.h"

namespace net {

static bool g_tcp_fastopen_enabled = false;

void set_tcp_fastopen_enabled(bool value) {
  g_tcp_fastopen_enabled = value;
}

bool is_tcp_fastopen_enabled() {
  return g_tcp_fastopen_enabled;
}

}  // namespace net
