// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/listen_socket.h"

namespace net {

ListenSocket::ListenSocket(ListenSocketDelegate* del) : socket_delegate_(del) {}

ListenSocket::~ListenSocket() {}

void ListenSocket::Send(const char* bytes, int len, bool append_linefeed) {
  SendInternal(bytes, len);
  if (append_linefeed)
    SendInternal("\r\n", 2);
}

void ListenSocket::Send(const std::string& str, bool append_linefeed) {
  Send(str.data(), static_cast<int>(str.length()), append_linefeed);
}

}  // namespace net
