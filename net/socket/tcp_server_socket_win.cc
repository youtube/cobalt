// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket_win.h"

#include <mstcpip.h>

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/winsock_init.h"
#include "net/base/winsock_util.h"
#include "net/socket/tcp_client_socket.h"

namespace net {

TCPServerSocketWin::TCPServerSocketWin(net::NetLog* net_log,
                                       const net::NetLog::Source& source)
    : socket_(INVALID_SOCKET),
      socket_event_(WSA_INVALID_EVENT),
      accept_socket_(NULL),
      accept_callback_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
  EnsureWinsockInit();
}

TCPServerSocketWin::~TCPServerSocketWin() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

int TCPServerSocketWin::Listen(const IPEndPoint& address, int backlog) {
  DCHECK(CalledOnValidThread());
  DCHECK_GT(backlog, 0);
  DCHECK_EQ(socket_, INVALID_SOCKET);
  DCHECK_EQ(socket_event_, WSA_INVALID_EVENT);

  socket_event_ = WSACreateEvent();
  if (socket_event_ == WSA_INVALID_EVENT) {
    PLOG(ERROR) << "WSACreateEvent()";
    return ERR_FAILED;
  }

  socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    PLOG(ERROR) << "socket() returned an error";
    return MapSystemError(WSAGetLastError());
  }

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(WSAGetLastError());
    Close();
    return result;
  }

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_INVALID_ARGUMENT;

  int result = bind(socket_, addr, addr_len);
  if (result < 0) {
    PLOG(ERROR) << "bind() returned an error";
    result = MapSystemError(WSAGetLastError());
    Close();
    return result;
  }

  result = listen(socket_, backlog);
  if (result < 0) {
    PLOG(ERROR) << "listen() returned an error";
    result = MapSystemError(WSAGetLastError());
    Close();
    return result;
  }

  return OK;
}

int TCPServerSocketWin::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);

  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (getsockname(socket_, addr, &addr_len))
    return MapSystemError(WSAGetLastError());
  if (!address->FromSockAddr(addr, addr_len))
    return ERR_FAILED;

  return OK;
}

int TCPServerSocketWin::Accept(
    scoped_ptr<ClientSocket>* socket, CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(callback);
  DCHECK(!accept_callback_);

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT, NULL);

  int result = AcceptInternal(socket);

  if (result == ERR_IO_PENDING) {
    // Start watching
    WSAEventSelect(socket_, socket_event_, FD_ACCEPT);
    accept_watcher_.StartWatching(socket_event_, this);

    accept_socket_ = socket;
    accept_callback_ = callback;
  }

  return result;
}

int TCPServerSocketWin::AcceptInternal(scoped_ptr<ClientSocket>* socket) {
  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  int result = accept(socket_, addr, &addr_len);
  if (result < 0) {
    int net_error = MapSystemError(WSAGetLastError());
    if (net_error != ERR_IO_PENDING)
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    return net_error;
  }

  IPEndPoint address;
  if (!address.FromSockAddr(addr, addr_len)) {
    NOTREACHED();
    if (closesocket(result) < 0)
      PLOG(ERROR) << "closesocket";
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, ERR_FAILED);
    return ERR_FAILED;
  }
  TCPClientSocket* tcp_socket = new TCPClientSocket(
      AddressList(address.address(), address.port(), false),
      net_log_.net_log(), net_log_.source());
  tcp_socket->AdoptSocket(result);
  socket->reset(tcp_socket);
  net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                    make_scoped_refptr(new NetLogStringParameter(
                        "address", address.ToString())));
  return OK;
}

void TCPServerSocketWin::Close() {
  if (socket_ != INVALID_SOCKET) {
    if (closesocket(socket_) < 0)
      PLOG(ERROR) << "closesocket";
    socket_ = INVALID_SOCKET;
  }

  if (socket_event_) {
    WSACloseEvent(socket_event_);
    socket_event_ = WSA_INVALID_EVENT;
  }
}

void TCPServerSocketWin::OnObjectSignaled(HANDLE object) {
  WSANETWORKEVENTS ev;
  if (WSAEnumNetworkEvents(socket_, socket_event_, &ev) == SOCKET_ERROR) {
    PLOG(ERROR) << "WSAEnumNetworkEvents()";
    return;
  }

  if (ev.lNetworkEvents & FD_ACCEPT) {
    int result = AcceptInternal(accept_socket_);
    if (result != ERR_IO_PENDING) {
      CompletionCallback* c = accept_callback_;
      accept_callback_ = NULL;
      accept_socket_ = NULL;
      c->Run(result);
    }
  }
}

}  // namespace net
