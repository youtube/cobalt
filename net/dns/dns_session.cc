// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_session.h"

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/time.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/dns/dns_config_service.h"
#include "net/dns/dns_socket_pool.h"

namespace net {

DnsSession::SocketLease::SocketLease(scoped_refptr<DnsSession> session,
                                     unsigned server_index,
                                     scoped_ptr<DatagramClientSocket> socket)
    : session_(session), server_index_(server_index), socket_(socket.Pass()) {}

DnsSession::SocketLease::~SocketLease() {
  session_->FreeSocket(server_index_, socket_.Pass());
}

DnsSession::DnsSession(const DnsConfig& config,
                       scoped_ptr<DnsSocketPool> socket_pool,
                       const RandIntCallback& rand_int_callback,
                       NetLog* net_log)
    : config_(config),
      socket_pool_(socket_pool.Pass()),
      rand_callback_(base::Bind(rand_int_callback, 0, kuint16max)),
      net_log_(net_log),
      server_index_(0) {
  socket_pool_->Initialize(&config_.nameservers, net_log);
}

DnsSession::~DnsSession() {
}

int DnsSession::NextQueryId() const {
  return rand_callback_.Run();
}

int DnsSession::NextFirstServerIndex() {
  int index = server_index_;
  if (config_.rotate)
    server_index_ = (server_index_ + 1) % config_.nameservers.size();
  return index;
}

base::TimeDelta DnsSession::NextTimeout(int attempt) {
  // The timeout doubles every full round (each nameserver once).
  // TODO(szym): Adapt timeout to observed RTT. http://crbug.com/110197
  return config_.timeout * (1 << (attempt / config_.nameservers.size()));
}

// Allocate a socket, already connected to the server address.
scoped_ptr<DnsSession::SocketLease> DnsSession::AllocateSocket(
    unsigned server_index,
    const NetLog::Source& source) {
  scoped_ptr<DatagramClientSocket> socket;

  socket = socket_pool_->AllocateSocket(server_index);
  if (!socket.get())
    return scoped_ptr<SocketLease>(NULL);

  socket->NetLog().BeginEvent(
      NetLog::TYPE_SOCKET_IN_USE,
      source.ToEventParametersCallback());

  SocketLease* lease = new SocketLease(this, server_index, socket.Pass());
  return scoped_ptr<SocketLease>(lease);
}

// Release a socket.
void DnsSession::FreeSocket(
    unsigned server_index,
    scoped_ptr<DatagramClientSocket> socket) {
  DCHECK(socket.get());

  socket->NetLog().EndEvent(NetLog::TYPE_SOCKET_IN_USE);

  socket_pool_->FreeSocket(server_index, socket.Pass());
}

}  // namespace net
