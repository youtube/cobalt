// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_client_socket.h"

#include "net/base/net_log.h"

namespace net {

UDPClientSocket::UDPClientSocket(
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : socket_(net_log, source) {
}

UDPClientSocket::~UDPClientSocket() {
}

int UDPClientSocket::Connect(const AddressList& address) {
  return socket_.Connect(address);
}

int UDPClientSocket::Read(IOBuffer* buf,
                          int buf_len,
                          CompletionCallback* callback) {
  return socket_.Read(buf, buf_len, callback);
}

int UDPClientSocket::Write(IOBuffer* buf,
                          int buf_len,
                          CompletionCallback* callback) {
  return socket_.Write(buf, buf_len, callback);
}

void UDPClientSocket::Close() {
  socket_.Close();
}

int UDPClientSocket::GetPeerAddress(AddressList* address) const {
  return socket_.GetPeerAddress(address);
}

int UDPClientSocket::GetLocalAddress(AddressList* address) const {
  return socket_.GetLocalAddress(address);
}


}  // namespace net
