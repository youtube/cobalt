// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/socket/tcp_server_socket_starboard.h"

#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/socket_net_log_params.h"
#include "net/socket/tcp_client_socket.h"
#include "starboard/socket.h"

namespace net {

namespace {
SbSocketAddressType TranslateAddressFamily(AddressFamily family) {
  switch (family) {
    case ADDRESS_FAMILY_IPV4:
      return kSbSocketAddressTypeIpv4;
      break;
    case ADDRESS_FAMILY_IPV6:
      return kSbSocketAddressTypeIpv6;
      break;
    default:
      NOTREACHED();
      return kSbSocketAddressTypeIpv4;
  }
}
}  // namespace

TCPServerSocketStarboard::TCPServerSocketStarboard(
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : socket_(kSbSocketInvalid),
      accept_socket_(NULL),
      reuse_address_(false),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
}

TCPServerSocketStarboard::~TCPServerSocketStarboard() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
}

void TCPServerSocketStarboard::AllowAddressReuse() {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(kSbSocketInvalid, socket_);

  reuse_address_ = true;
}

int TCPServerSocketStarboard::Listen(const IPEndPoint& address, int backlog) {
  DCHECK(CalledOnValidThread());
  DCHECK_GT(backlog, 0);
  DCHECK_EQ(kSbSocketInvalid, socket_);

  socket_ = SbSocketCreate(TranslateAddressFamily(address.GetFamily()),
                           kSbSocketProtocolTcp);
  if (!SbSocketIsValid(socket_)) {
    DPLOG(ERROR) << "SbSocketCreate() returned an error";
    return MapLastSystemError();
  }

  int result = SetSocketOptions();
  if (result != OK) {
    return result;
  }

  SbSocketAddress sb_address;
  if (!address.ToSbSocketAddress(&sb_address)) {
    return ERR_INVALID_ARGUMENT;
  }

  SbSocketError error = SbSocketBind(socket_, &sb_address);
  if (error != kSbSocketOk) {
    DLOG(ERROR) << "SbSocketBind() returned an error";
    result = MapLastSocketError(socket_);
    Close();
    return result;
  }

  error = SbSocketListen(socket_);
  if (error != kSbSocketOk) {
    DLOG(ERROR) << "SbSocketListen() returned an error";
    result = MapLastSocketError(socket_);
    Close();
    return result;
  }

  return OK;
}

int TCPServerSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);

  SbSocketAddress sb_address;
  if (!SbSocketGetLocalAddress(socket_, &sb_address)) {
    return MapLastSocketError(socket_);
  }

  if (!address->FromSbSocketAddress(&sb_address)) {
    return ERR_FAILED;
  }

  return OK;
}

int TCPServerSocketStarboard::Accept(scoped_ptr<StreamSocket>* socket,
                                     const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(socket);
  DCHECK(!callback.is_null());
  DCHECK(accept_callback_.is_null());

  net_log_.BeginEvent(NetLog::TYPE_TCP_ACCEPT);

  int result = AcceptInternal(socket);

  if (result == ERR_IO_PENDING) {
    if (!MessageLoopForIO::current()->Watch(socket_, true,
                                            MessageLoopForIO::WATCH_READ,
                                            &accept_socket_watcher_, this)) {
      DLOG(ERROR) << "WatchSocket failed on read";
      return MapLastSocketError(socket_);
    }

    accept_socket_ = socket;
    accept_callback_ = callback;
  }

  return result;
}

int TCPServerSocketStarboard::SetSocketOptions() {
  if (reuse_address_) {
    if (!SbSocketSetReuseAddress(socket_, true)) {
      return MapLastSocketError(socket_);
    }
  }
  return OK;
}

int TCPServerSocketStarboard::AcceptInternal(scoped_ptr<StreamSocket>* socket) {
  SbSocket new_socket = SbSocketAccept(socket_);
  if (!SbSocketIsValid(new_socket)) {
    int net_error = MapLastSocketError(socket_);
    if (net_error != ERR_IO_PENDING) {
      net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, net_error);
    }
    return net_error;
  }

  SbSocketAddress sb_address;
  char b;
  int received = SbSocketReceiveFrom(socket_, &b, 0, &sb_address);
  if (received != 0) {
    SbSocketDestroy(new_socket);
    return ERR_FAILED;
  }

  IPEndPoint address;
  if (!address.FromSbSocketAddress(&sb_address)) {
    SbSocketDestroy(new_socket);
    return ERR_FAILED;
  }

  scoped_ptr<TCPClientSocket> tcp_socket(new TCPClientSocket(
      AddressList(address), net_log_.net_log(), net_log_.source()));
  int adopt_result = tcp_socket->AdoptSocket(new_socket);
  if (adopt_result != OK) {
    if (!SbSocketDestroy(new_socket)) {
      DPLOG(ERROR) << "SbSocketDestroy";
    }
    net_log_.EndEventWithNetErrorCode(NetLog::TYPE_TCP_ACCEPT, adopt_result);
    return adopt_result;
  }

  socket->reset(tcp_socket.release());
  net_log_.EndEvent(NetLog::TYPE_TCP_ACCEPT,
                    CreateNetLogIPEndPointCallback(&address));
  return OK;
}

void TCPServerSocketStarboard::Close() {
  if (SbSocketIsValid(socket_)) {
    bool ok = accept_socket_watcher_.StopWatchingSocket();
    DCHECK(ok);
    if (!SbSocketDestroy(socket_)) {
      DPLOG(ERROR) << "SbSocketDestroy";
    }
    socket_ = kSbSocketInvalid;
  }
}

void TCPServerSocketStarboard::OnSocketReadyToRead(SbSocket socket) {
  DCHECK(CalledOnValidThread());

  int result = AcceptInternal(accept_socket_);
  if (result != ERR_IO_PENDING) {
    accept_socket_ = NULL;
    bool ok = accept_socket_watcher_.StopWatchingSocket();
    DCHECK(ok);
    CompletionCallback callback = accept_callback_;
    accept_callback_.Reset();
    callback.Run(result);
  }
}

void TCPServerSocketStarboard::OnSocketReadyToWrite(SbSocket socket) {
  NOTREACHED();
}

}  // namespace net
