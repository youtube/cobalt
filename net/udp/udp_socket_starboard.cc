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

// Adapted from udp_socket_libevent.cc

#include "net/udp/udp_socket_starboard.h"

#include "base/callback.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "base/rand_util.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#include "net/udp/udp_net_log_parameters.h"
#include "starboard/socket.h"
#include "starboard/system.h"

namespace {

static const int kBindRetries = 10;
static const int kPortStart = 1024;
static const int kPortEnd = 65535;

}  // namespace net

namespace net {

UDPSocketStarboard::UDPSocketStarboard(DatagramSocket::BindType bind_type,
                                       const RandIntCallback& rand_int_cb,
                                       net::NetLog* net_log,
                                       const net::NetLog::Source& source)
    : socket_(kSbSocketInvalid),
      socket_options_(0),
      bind_type_(bind_type),
      rand_int_cb_(rand_int_cb),
      read_watcher_(this),
      write_watcher_(this),
      read_buf_len_(0),
      recv_from_address_(NULL),
      write_buf_len_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_UDP_SOCKET)) {
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE,
                      source.ToEventParametersCallback());
  if (bind_type == DatagramSocket::RANDOM_BIND)
    DCHECK(!rand_int_cb.is_null());
}

UDPSocketStarboard::~UDPSocketStarboard() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE);
}

void UDPSocketStarboard::Close() {
  DCHECK(CalledOnValidThread());

  if (!is_connected())
    return;

  // Zero out any pending read/write callback state.
  read_buf_ = NULL;
  read_buf_len_ = 0;
  read_callback_.Reset();
  recv_from_address_ = NULL;
  write_buf_ = NULL;
  write_buf_len_ = 0;
  write_callback_.Reset();
  send_to_address_.reset();

  bool ok = read_socket_watcher_.StopWatchingSocket();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingSocket();
  DCHECK(ok);

  if (!SbSocketDestroy(socket_)) {
    DPLOG(ERROR) << "SbSocketDestroy";
  }

  socket_ = kSbSocketInvalid;
}

Error UDPSocketStarboard::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  DCHECK(remote_address_);
  *address = *remote_address_;
  return OK;
}

Error UDPSocketStarboard::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!local_address_.get()) {
    SbSocketAddress address;
    if (!SbSocketGetLocalAddress(socket_, &address))
      return MapLastSocketError(socket_);
    scoped_ptr<IPEndPoint> endpoint(new IPEndPoint());
    if (!endpoint->FromSbSocketAddress(&address))
      return ERR_FAILED;
    local_address_.reset(endpoint.release());
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketStarboard::Read(IOBuffer* buf,
                             int buf_len,
                             const CompletionCallback& callback) {
  return RecvFrom(buf, buf_len, NULL, callback);
}

int UDPSocketStarboard::RecvFrom(IOBuffer* buf,
                                 int buf_len,
                                 IPEndPoint* address,
                                 const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kSbSocketInvalid, socket_);
  DCHECK(read_callback_.is_null());
  DCHECK(!recv_from_address_);
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!MessageLoopForIO::current()->Watch(
          socket_, true, MessageLoopForIO::WATCH_READ, &read_socket_watcher_,
          &read_watcher_)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    Error result = MapLastSocketError(socket_);
    LogRead(result, NULL, NULL);
    return result;
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  recv_from_address_ = address;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketStarboard::Write(IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  return SendToOrWrite(buf, buf_len, NULL, callback);
}

int UDPSocketStarboard::SendTo(IOBuffer* buf,
                               int buf_len,
                               const IPEndPoint& address,
                               const CompletionCallback& callback) {
  return SendToOrWrite(buf, buf_len, &address, callback);
}

int UDPSocketStarboard::SendToOrWrite(IOBuffer* buf,
                                      int buf_len,
                                      const IPEndPoint* address,
                                      const CompletionCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kSbSocketInvalid, socket_);
  DCHECK(write_callback_.is_null());
  DCHECK(!callback.is_null());  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int result = InternalSendTo(buf, buf_len, address);
  if (result != ERR_IO_PENDING)
    return result;

  if (!MessageLoopForIO::current()->Watch(
          socket_, true, MessageLoopForIO::WATCH_WRITE, &write_socket_watcher_,
          &write_watcher_)) {
    DVLOG(1) << "Watch failed on write, error "
             << SbSocketGetLastError(socket_);
    Error result = MapLastSocketError(socket_);
    LogWrite(result, NULL, NULL);
    return result;
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  DCHECK(!send_to_address_.get());
  if (address) {
    send_to_address_.reset(new IPEndPoint(*address));
  }
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

Error UDPSocketStarboard::Connect(const IPEndPoint& address) {
  net_log_.BeginEvent(NetLog::TYPE_UDP_CONNECT,
                      CreateNetLogUDPConnectCallback(&address));
  Error rv = InternalConnect(address);
  net_log_.EndEventWithNetErrorCode(NetLog::TYPE_UDP_CONNECT, rv);
  return rv;
}

Error UDPSocketStarboard::InternalConnect(const IPEndPoint& address) {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());
  Error error = CreateSocket(address);
  if (error != OK)
    return error;

  if (bind_type_ == DatagramSocket::RANDOM_BIND)
    error = RandomBind(address);
  // else connect() does the DatagramSocket::DEFAULT_BIND

  if (error != OK)
    return error;

  SbSocketAddress sb_address;
  if (!address.ToSbSocketAddress(&sb_address))
    return ERR_FAILED;

  SbSocketError sb_error = SbSocketConnect(socket_, &sb_address);
  if (sb_error != kSbSocketOk)
    return MapSocketError(sb_error);

  remote_address_.reset(new IPEndPoint(address));
  return OK;
}

Error UDPSocketStarboard::Bind(const IPEndPoint& address) {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());
  Error error = CreateSocket(address);
  if (error != OK)
    return error;
  error = SetSocketOptions();
  if (error != OK)
    return error;
  error = DoBind(address);
  if (error != OK)
    return error;
  local_address_.reset();
  return OK;
}

bool UDPSocketStarboard::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  bool result = SbSocketSetReceiveBufferSize(socket_, size);
  DCHECK(result) << "Could not " << __FUNCTION__ << ": "
                 << SbSocketGetLastError(socket_);
  return result;
}

bool UDPSocketStarboard::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  bool result = SbSocketSetSendBufferSize(socket_, size);
  DCHECK(result) << "Could not " << __FUNCTION__ << ": "
                 << SbSocketGetLastError(socket_);
  return result;
}

void UDPSocketStarboard::AllowAddressReuse() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());

  socket_options_ |= SOCKET_OPTION_REUSE_ADDRESS;
}

void UDPSocketStarboard::AllowBroadcast() {
  DCHECK(CalledOnValidThread());
  DCHECK(!is_connected());

  socket_options_ |= SOCKET_OPTION_BROADCAST;
}

void UDPSocketStarboard::ReadWatcher::OnSocketReadyToRead(SbSocket /*socket*/) {
  if (!socket_->read_callback_.is_null())
    socket_->DidCompleteRead();
}

void UDPSocketStarboard::WriteWatcher::OnSocketReadyToWrite(
    SbSocket /*socket*/) {
  if (!socket_->write_callback_.is_null())
    socket_->DidCompleteWrite();
}

void UDPSocketStarboard::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!read_callback_.is_null());

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback c = read_callback_;
  read_callback_.Reset();
  c.Run(rv);
}

void UDPSocketStarboard::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(!write_callback_.is_null());

  // Run may result in Write being called.
  base::ResetAndReturn(&write_callback_).Run(rv);
}

void UDPSocketStarboard::DidCompleteRead() {
  int result = InternalRecvFrom(read_buf_, read_buf_len_, recv_from_address_);
  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    recv_from_address_ = NULL;
    bool ok = read_socket_watcher_.StopWatchingSocket();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

void UDPSocketStarboard::LogRead(int result,
                                 const char* bytes,
                                 const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLog::TYPE_UDP_RECEIVE_ERROR, result);
    return;
  }

  if (net_log_.IsLoggingAllEvents()) {
    DCHECK(address);
    net_log_.AddEvent(
        NetLog::TYPE_UDP_BYTES_RECEIVED,
        CreateNetLogUDPDataTranferCallback(result, bytes, address));
  }

  base::StatsCounter read_bytes("udp.read_bytes");
  read_bytes.Add(result);
}

Error UDPSocketStarboard::CreateSocket(const IPEndPoint& address) {
  SbSocketAddressType address_type =
      (address.GetFamily() == ADDRESS_FAMILY_IPV6 ? kSbSocketAddressTypeIpv6
                                                  : kSbSocketAddressTypeIpv4);
  socket_ = SbSocketCreate(address_type, kSbSocketProtocolUdp);
  if (!SbSocketIsValid(socket_))
    return MapLastSystemError();
  return OK;
}

void UDPSocketStarboard::DidCompleteWrite() {
  int result =
      InternalSendTo(write_buf_, write_buf_len_, send_to_address_.get());

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    send_to_address_.reset();
    write_socket_watcher_.StopWatchingSocket();
    DoWriteCallback(result);
  }
}

void UDPSocketStarboard::LogWrite(int result,
                                  const char* bytes,
                                  const IPEndPoint* address) const {
  if (result < 0) {
    net_log_.AddEventWithNetErrorCode(NetLog::TYPE_UDP_SEND_ERROR, result);
    return;
  }

  if (net_log_.IsLoggingAllEvents()) {
    net_log_.AddEvent(
        NetLog::TYPE_UDP_BYTES_SENT,
        CreateNetLogUDPDataTranferCallback(result, bytes, address));
  }

  base::StatsCounter write_bytes("udp.write_bytes");
  write_bytes.Add(result);
}

int UDPSocketStarboard::InternalRecvFrom(IOBuffer* buf,
                                         int buf_len,
                                         IPEndPoint* address) {
  SbSocketAddress sb_address;
  int bytes_transferred =
      SbSocketReceiveFrom(socket_, buf->data(), buf_len, &sb_address);
  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    if (address && !address->FromSbSocketAddress(&sb_address)) {
      result = ERR_FAILED;
    }
  } else {
    result = MapLastSocketError(socket_);
  }

  if (result != ERR_IO_PENDING) {
    IPEndPoint log_address;
    if (log_address.FromSbSocketAddress(&sb_address)) {
      LogRead(result, buf->data(), NULL);
    } else {
      LogRead(result, buf->data(), &log_address);
    }
  }

  return result;
}

int UDPSocketStarboard::InternalSendTo(IOBuffer* buf,
                                       int buf_len,
                                       const IPEndPoint* address) {
  SbSocketAddress sb_address;
  if (address && !address->ToSbSocketAddress(&sb_address)) {
    int result = ERR_FAILED;
    LogWrite(result, NULL, NULL);
    return result;
  }

  int result = SbSocketSendTo(socket_, buf->data(), buf_len, &sb_address);

  if (result < 0)
    result = MapLastSocketError(socket_);

  if (result != ERR_IO_PENDING)
    LogWrite(result, buf->data(), address);

  return result;
}

Error UDPSocketStarboard::SetSocketOptions() {
  if (socket_options_ & SOCKET_OPTION_REUSE_ADDRESS) {
    if (!SbSocketSetReuseAddress(socket_, true)) {
      return MapLastSocketError(socket_);
    }
  }

  if (socket_options_ & SOCKET_OPTION_BROADCAST) {
    if (!SbSocketSetBroadcast(socket_, true)) {
      return MapLastSocketError(socket_);
    }
  }

  return OK;
}

Error UDPSocketStarboard::DoBind(const IPEndPoint& address) {
  SbSocketAddress sb_address;
  if (!address.ToSbSocketAddress(&sb_address)) {
    return ERR_UNEXPECTED;
  }

  SbSocketError rv = SbSocketBind(socket_, &sb_address);
  return rv != kSbSocketOk ? MapLastSystemError() : OK;
}

Error UDPSocketStarboard::RandomBind(const IPEndPoint& address) {
  DCHECK(bind_type_ == DatagramSocket::RANDOM_BIND && !rand_int_cb_.is_null());

  // Construct IPAddressNumber of appropriate size (IPv4 or IPv6) of 0s.
  IPAddressNumber ip(address.address().size());

  for (int i = 0; i < kBindRetries; ++i) {
    Error rv = DoBind(IPEndPoint(ip, rand_int_cb_.Run(kPortStart, kPortEnd)));
    if (rv == OK || rv != ERR_ADDRESS_IN_USE)
      return rv;
  }

  return DoBind(IPEndPoint(ip, 0));
}

}  // namespace net
