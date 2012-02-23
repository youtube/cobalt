// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_server_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <netinet/in.h>
#endif

#include "base/eintr_wrapper.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"

namespace net {

namespace {

const int kInvalidSocket = -1;

}  // namespace

TCPServerSocketLibevent::TCPServerSocketLibevent(
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : socket_(kInvalidSocket),
      accept_socket_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
}

TCPServerSocketLibevent::~TCPServerSocketLibevent() {
  if (socket_ != kInvalidSocket)
    Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

int TCPServerSocketLibevent::Listen(const IPEndPoint& address, int backlog) {
  DCHECK(CalledOnValidThread());
  DCHECK_GT(backlog, 0);
  DCHECK_EQ(socket_, kInvalidSocket);

  socket_ = socket(address.GetFamily(), SOCK_STREAM, IPPROTO_TCP);
  if (socket_ < 0) {
    PLOG(ERROR) << "socket() returned an error";
    return MapSystemError(errno);
  }

  if (SetNonBlocking(socket_)) {
    int result = MapSystemError(errno);
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
    result = MapSystemError(errno);
    Close();
    return result;
  }

  result = listen(socket_, backlog);
  if (result < 0) {
    PLOG(ERROR) << "listen() returned an error";
    result = MapSystemError(errno);
    Close();
    return result;
  }

  return OK;
}

int TCPServerSocketLibevent::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);

  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (getsockname(socket_, addr, &addr_len) < 0)
    return MapSystemError(errno);
  if (!address->FromSockAddr(addr, addr_len))
    return ERR_FAILED;

  return OK;
}

int TCPServerSocketLibevent::Accept(
    scoped_ptr<StreamSocket>* socket, const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT, NULL);

  int result = AcceptInternal(socket);

  if (result == ERR_IO_PENDING) {
    if (!MessageLoopForIO::current()->WatchFileDescriptor(
            socket_, true, MessageLoopForIO::WATCH_READ,
            &accept_socket_watcher_, this)) {
      PLOG(ERROR) << "WatchFileDescriptor failed on read";
      return MapSystemError(errno);
    }

    accept_socket_ = socket;
    accept_callback_ = callback;
  }

  return result;
}

int TCPServerSocketLibevent::AcceptInternal(
    scoped_ptr<StreamSocket>* socket) {
  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  int new_socket = HANDLE_EINTR(accept(socket_, addr, &addr_len));
  if (new_socket < 0) {
    int net_error = MapSystemError(errno);
    if (net_error != ERR_IO_PENDING)
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    return net_error;
  }

  IPEndPoint address;
  if (!address.FromSockAddr(addr, addr_len)) {
    NOTREACHED();
    if (HANDLE_EINTR(close(new_socket)) < 0)
      PLOG(ERROR) << "close";
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, ERR_FAILED);
    return ERR_FAILED;
  }
  scoped_ptr<TCPClientSocket> tcp_socket(new TCPClientSocket(
      AddressList::CreateFromIPAddress(address.address(), address.port()),
      net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->AdoptSocket(new_socket);
  if (adopt_result != OK) {
    if (HANDLE_EINTR(close(new_socket)) < 0)
      PLOG(ERROR) << "close";
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, adopt_result);
    return adopt_result;
  }
  socket->reset(tcp_socket.release());
  net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                    make_scoped_refptr(new NetLogStringParameter(
                        "address", address.ToString())));
  return OK;
}

void TCPServerSocketLibevent::Close() {
  if (socket_ != kInvalidSocket) {
    bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    if (HANDLE_EINTR(close(socket_)) < 0)
      PLOG(ERROR) << "close";
    socket_ = kInvalidSocket;
  }
}

void TCPServerSocketLibevent::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(CalledOnValidThread());

  int result = AcceptInternal(accept_socket_);
  if (result != ERR_IO_PENDING) {
    accept_socket_ = NULL;
    bool ok = accept_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    accept_callback_.Run(result);
    accept_callback_.Reset();
  }
}

void TCPServerSocketLibevent::OnFileCanWriteWithoutBlocking(int fd) {
  NOTREACHED();
}

}  // namespace net
