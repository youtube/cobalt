// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_server_socket.h"

namespace net {

UDPServerSocket::UDPServerSocket(net::NetLog* net_log,
                                 const net::NetLog::Source& source)
    : socket_(net_log, source) {
}

UDPServerSocket::~UDPServerSocket() {
}

int UDPServerSocket::Listen(const AddressList& address) {
  return socket_.Bind(address);
}

int UDPServerSocket::RecvFrom(IOBuffer* buf,
                              int buf_len,
                              struct sockaddr* address,
                              socklen_t* address_length,
                              CompletionCallback* callback) {
  return socket_.RecvFrom(buf, buf_len, address, address_length, callback);
}

int UDPServerSocket::SendTo(IOBuffer* buf,
                            int buf_len,
                            const struct sockaddr* address,
                            socklen_t address_length,
                            CompletionCallback* callback) {
  return socket_.SendTo(buf, buf_len, address, address_length, callback);
}

void UDPServerSocket::Close() {
  socket_.Close();
}

int UDPServerSocket::GetPeerAddress(AddressList* address) const {
  return socket_.GetPeerAddress(address);
}

int UDPServerSocket::GetLocalAddress(AddressList* address) const {
  return socket_.GetLocalAddress(address);
}


}  // namespace net
