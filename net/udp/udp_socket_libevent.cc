// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/udp/udp_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/metrics/stats_counters.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#if defined(OS_POSIX)
#include <netinet/in.h>
#endif
#if defined(USE_SYSTEM_LIBEVENT)
#include <event.h>
#else
#include "third_party/libevent/event.h"
#endif

namespace net {

UDPSocketLibevent::UDPSocketLibevent(net::NetLog* net_log,
                                     const net::NetLog::Source& source)
    : socket_(kInvalidSocket),
      read_watcher_(this),
      write_watcher_(this),
      read_buf_len_(0),
      recv_from_address_(NULL),
      write_buf_len_(0),
      read_callback_(NULL),
      write_callback_(NULL),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
}

UDPSocketLibevent::~UDPSocketLibevent() {
  Close();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

void UDPSocketLibevent::Close() {
  DCHECK(CalledOnValidThread());

  if (!is_connected())
    return;

  // Zero out any pending read/write callback state.
  read_buf_ = NULL;
  read_buf_len_ = 0;
  read_callback_ = NULL;
  recv_from_address_ = NULL;
  write_buf_ = NULL;
  write_buf_len_ = 0;
  write_callback_ = NULL;
  send_to_address_.reset();

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);

  if (HANDLE_EINTR(close(socket_)) < 0)
    PLOG(ERROR) << "close";

  socket_ = kInvalidSocket;
}

int UDPSocketLibevent::GetPeerAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!remote_address_.get()) {
    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);
    struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
    if (getpeername(socket_, addr, &addr_len))
      return MapSystemError(errno);
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(addr, addr_len))
      return ERR_FAILED;
    remote_address_.reset(address.release());
  }

  *address = *remote_address_;
  return OK;
}

int UDPSocketLibevent::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!is_connected())
    return ERR_SOCKET_NOT_CONNECTED;

  if (!local_address_.get()) {
    struct sockaddr_storage addr_storage;
    socklen_t addr_len = sizeof(addr_storage);
    struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
    if (getsockname(socket_, addr, &addr_len))
      return MapSystemError(errno);
    scoped_ptr<IPEndPoint> address(new IPEndPoint());
    if (!address->FromSockAddr(addr, addr_len))
      return ERR_FAILED;
    local_address_.reset(address.release());
  }

  *address = *local_address_;
  return OK;
}

int UDPSocketLibevent::Read(IOBuffer* buf,
                            int buf_len,
                            CompletionCallback* callback) {
  return RecvFrom(buf, buf_len, NULL, callback);
}

int UDPSocketLibevent::RecvFrom(IOBuffer* buf,
                                int buf_len,
                                IPEndPoint* address,
                                CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!read_callback_);
  DCHECK(!recv_from_address_);
  DCHECK(callback);  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nread = InternalRecvFrom(buf, buf_len, address);
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    return MapSystemError(errno);
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  recv_from_address_ = address;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketLibevent::Write(IOBuffer* buf,
                             int buf_len,
                             CompletionCallback* callback) {
  return SendToOrWrite(buf, buf_len, NULL, callback);
}

int UDPSocketLibevent::SendTo(IOBuffer* buf,
                              int buf_len,
                              const IPEndPoint& address,
                              CompletionCallback* callback) {
  return SendToOrWrite(buf, buf_len, &address, callback);
}

int UDPSocketLibevent::SendToOrWrite(IOBuffer* buf,
                                     int buf_len,
                                     const IPEndPoint* address,
                                     CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!write_callback_);
  DCHECK(callback);  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nwrite = InternalSendTo(buf, buf_len, address);
  if (nwrite >= 0) {
    static base::StatsCounter write_bytes("udp.write_bytes");
    write_bytes.Add(nwrite);
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return MapSystemError(errno);

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DVLOG(1) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapSystemError(errno);
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

int UDPSocketLibevent::Connect(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = HANDLE_EINTR(connect(socket_, addr, addr_len));
  if (rv < 0)
    return MapSystemError(errno);

  remote_address_.reset(new IPEndPoint(address));
  return rv;
}

int UDPSocketLibevent::Bind(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!local_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return rv;

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = bind(socket_, addr, addr_len);
  if (rv < 0)
    return MapSystemError(errno);

  local_address_.reset(new IPEndPoint(address));
  return rv;
}

void UDPSocketLibevent::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void UDPSocketLibevent::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void UDPSocketLibevent::DidCompleteRead() {
  int result = InternalRecvFrom(read_buf_, read_buf_len_, recv_from_address_);
  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    recv_from_address_ = NULL;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

int UDPSocketLibevent::CreateSocket(const IPEndPoint& address) {
  socket_ = socket(address.GetFamily(), SOCK_DGRAM, 0);
  if (socket_ == kInvalidSocket)
    return MapSystemError(errno);
  if (SetNonBlocking(socket_)) {
    const int err = MapSystemError(errno);
    Close();
    return err;
  }
  return OK;
}

void UDPSocketLibevent::DidCompleteWrite() {
  int result = InternalSendTo(write_buf_, write_buf_len_,
                              send_to_address_.get());
  if (result >= 0) {
    static base::StatsCounter write_bytes("udp.write_bytes");
    write_bytes.Add(result);
  } else {
    result = MapSystemError(errno);
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    send_to_address_.reset();
    write_socket_watcher_.StopWatchingFileDescriptor();
    DoWriteCallback(result);
  }
}

int UDPSocketLibevent::InternalRecvFrom(IOBuffer* buf, int buf_len,
                                        IPEndPoint* address) {
  int bytes_transferred;
  int flags = 0;

  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  bytes_transferred =
      HANDLE_EINTR(recvfrom(socket_,
                            buf->data(),
                            buf_len,
                            flags,
                            addr,
                            &addr_len));
  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    static base::StatsCounter read_bytes("udp.read_bytes");
    read_bytes.Add(bytes_transferred);
    if (address) {
      if (!address->FromSockAddr(addr, addr_len))
        result = ERR_FAILED;
    }
  } else {
    result = MapSystemError(errno);
  }
  return result;
}

int UDPSocketLibevent::InternalSendTo(IOBuffer* buf, int buf_len,
                                      const IPEndPoint* address) {
  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  if (!address) {
    addr = NULL;
    addr_len = 0;
  } else {
    if (!address->ToSockAddr(addr, &addr_len))
      return ERR_FAILED;
  }

  return HANDLE_EINTR(sendto(socket_,
                             buf->data(),
                             buf_len,
                             0,
                             addr,
                             addr_len));
}

}  // namespace net
