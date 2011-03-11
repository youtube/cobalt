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

namespace {

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixError(int os_error) {
  // There are numerous posix error codes, but these are the ones we thus far
  // find interesting.
  switch (os_error) {
    case EAGAIN:
#if EWOULDBLOCK != EAGAIN
    case EWOULDBLOCK:
#endif
      return ERR_IO_PENDING;
    case EACCES:
      return ERR_ACCESS_DENIED;
    case ENETDOWN:
      return ERR_INTERNET_DISCONNECTED;
    case ETIMEDOUT:
      return ERR_TIMED_OUT;
    case ECONNRESET:
    case ENETRESET:  // Related to keep-alive
    case EPIPE:
      return ERR_CONNECTION_RESET;
    case ECONNABORTED:
      return ERR_CONNECTION_ABORTED;
    case ECONNREFUSED:
      return ERR_CONNECTION_REFUSED;
    case EHOSTUNREACH:
    case EHOSTDOWN:
    case ENETUNREACH:
      return ERR_ADDRESS_UNREACHABLE;
    case EADDRNOTAVAIL:
      return ERR_ADDRESS_INVALID;
    case EMSGSIZE:
      return ERR_MSG_TOO_BIG;
    case 0:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << os_error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

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
  if (!is_connected())
    return;

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
      return MapPosixError(errno);
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
      return MapPosixError(errno);
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
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!read_callback_);
  DCHECK(callback);  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  read_buf_ = buf;
  read_buf_len_ = buf_len;

  int nread = InternalRead();
  if (nread != ERR_IO_PENDING)
    return nread;

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    PLOG(ERROR) << "WatchFileDescriptor failed on read";
    return MapPosixError(errno);
  }

  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketLibevent::RecvFrom(IOBuffer* buf,
                                int buf_len,
                                IPEndPoint* address,
                                CompletionCallback* callback) {
  DCHECK(!recv_from_address_);
  recv_from_address_ = address;
  return Read(buf, buf_len, callback);
}

int UDPSocketLibevent::SendTo(IOBuffer* buf,
                              int buf_len,
                              const IPEndPoint& address,
                              CompletionCallback* callback) {
  send_to_address_.reset(new IPEndPoint(address));
  return Write(buf, buf_len, callback);
}

int UDPSocketLibevent::Write(IOBuffer* buf,
                             int buf_len,
                             CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!write_callback_);
  DCHECK(callback);  // Synchronous operation not supported
  DCHECK_GT(buf_len, 0);

  int nwrite = InternalWrite(buf, buf_len);
  if (nwrite >= 0) {
    static base::StatsCounter write_bytes("udp.write_bytes");
    write_bytes.Add(nwrite);
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return MapPosixError(errno);

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DVLOG(1) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapPosixError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int UDPSocketLibevent::Connect(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!remote_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return MapPosixError(rv);

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = HANDLE_EINTR(connect(socket_, addr, addr_len));
  if (rv < 0)
    return MapPosixError(rv);

  remote_address_.reset(new IPEndPoint(address));
  return rv;
}

int UDPSocketLibevent::Bind(const IPEndPoint& address) {
  DCHECK(!is_connected());
  DCHECK(!local_address_.get());
  int rv = CreateSocket(address);
  if (rv < 0)
    return MapPosixError(rv);

  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);
  if (!address.ToSockAddr(addr, &addr_len))
    return ERR_FAILED;

  rv = bind(socket_, addr, addr_len);
  if (rv < 0)
    return MapPosixError(rv);

  local_address_.reset(new IPEndPoint(address));
  return rv;
}

void UDPSocketLibevent::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  recv_from_address_ = NULL;
  c->Run(rv);
}

void UDPSocketLibevent::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  send_to_address_.reset();
  c->Run(rv);
}

void UDPSocketLibevent::DidCompleteRead() {
  int result = InternalRead();
  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

int UDPSocketLibevent::CreateSocket(const IPEndPoint& address) {
  socket_ = socket(address.GetFamily(), SOCK_DGRAM, 0);
  if (socket_ == kInvalidSocket)
    return errno;
  if (SetNonBlocking(socket_)) {
    const int err = errno;
    Close();
    return err;
  }
  return OK;
}

void UDPSocketLibevent::DidCompleteWrite() {
  int result = InternalWrite(write_buf_, write_buf_len_);
  if (result >= 0) {
    static base::StatsCounter write_bytes("udp.write_bytes");
    write_bytes.Add(result);
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    write_buf_ = NULL;
    write_buf_len_ = 0;
    write_socket_watcher_.StopWatchingFileDescriptor();
    DoWriteCallback(result);
  }
}

int UDPSocketLibevent::InternalRead() {
  int bytes_transferred;
  int flags = 0;

  struct sockaddr_storage addr_storage;
  socklen_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  bytes_transferred =
      HANDLE_EINTR(recvfrom(socket_,
                            read_buf_->data(),
                            read_buf_len_,
                            flags,
                            addr,
                            &addr_len));
  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    static base::StatsCounter read_bytes("udp.read_bytes");
    read_bytes.Add(bytes_transferred);
    if (recv_from_address_) {
      if (!recv_from_address_->FromSockAddr(addr, addr_len))
        result = ERR_FAILED;
    }
  } else {
    result = MapPosixError(errno);
  }
  return result;
}
int UDPSocketLibevent::InternalWrite(IOBuffer* buf, int buf_len) {
  struct sockaddr_storage addr_storage;
  size_t addr_len = sizeof(addr_storage);
  struct sockaddr* addr = reinterpret_cast<struct sockaddr*>(&addr_storage);

  if (!send_to_address_.get()) {
    addr = NULL;
    addr_len = 0;
  } else {
    if (!send_to_address_->ToSockAddr(addr, &addr_len))
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
