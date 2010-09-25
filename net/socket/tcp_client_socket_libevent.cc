// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#if defined(OS_POSIX)
#include <netinet/in.h>
#endif

#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stats_counters.h"
#include "base/string_util.h"
#include "net/base/address_list_net_log_param.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_util.h"
#if defined(USE_SYSTEM_LIBEVENT)
#include <event.h>
#else
#include "third_party/libevent/event.h"
#endif

namespace net {

namespace {

const int kInvalidSocket = -1;

// DisableNagle turns off buffering in the kernel. By default, TCP sockets will
// wait up to 200ms for more data to complete a packet before transmitting.
// After calling this function, the kernel will not wait. See TCP_NODELAY in
// `man 7 tcp`.
int DisableNagle(int fd) {
  int on = 1;
  return setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

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
    case 0:
      return OK;
    default:
      LOG(WARNING) << "Unknown error " << os_error
                   << " mapped to net::ERR_FAILED";
      return ERR_FAILED;
  }
}

int MapConnectError(int os_error) {
  switch (os_error) {
    case ETIMEDOUT:
      return ERR_CONNECTION_TIMED_OUT;
    default: {
      int net_error = MapPosixError(os_error);
      if (net_error == ERR_FAILED)
        return ERR_CONNECTION_FAILED;  // More specific than ERR_FAILED.
      return net_error;
    }
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TCPClientSocketLibevent::TCPClientSocketLibevent(
    const AddressList& addresses,
    net::NetLog* net_log,
    const net::NetLog::Source& source)
    : socket_(kInvalidSocket),
      addresses_(addresses),
      current_ai_(NULL),
      read_watcher_(this),
      write_watcher_(this),
      read_callback_(NULL),
      write_callback_(NULL),
      next_connect_state_(CONNECT_STATE_NONE),
      connect_os_error_(0),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)) {
  scoped_refptr<NetLog::EventParameters> params;
  if (source.is_valid())
    params = new NetLogSourceParameter("source_dependency", source);
  net_log_.BeginEvent(NetLog::TYPE_SOCKET_ALIVE, params);
}

TCPClientSocketLibevent::~TCPClientSocketLibevent() {
  Disconnect();
  net_log_.EndEvent(NetLog::TYPE_SOCKET_ALIVE, NULL);
}

int TCPClientSocketLibevent::Connect(CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());

  // If already connected, then just return OK.
  if (socket_ != kInvalidSocket)
    return OK;

  static StatsCounter connects("tcp.connect");
  connects.Increment();

  DCHECK(!waiting_connect());

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT,
                      new AddressListNetLogParam(addresses_));

  // We will try to connect to each address in addresses_. Start with the
  // first one in the list.
  next_connect_state_ = CONNECT_STATE_CONNECT;
  current_ai_ = addresses_.head();

  int rv = DoConnectLoop(OK);
  if (rv == ERR_IO_PENDING) {
    // Synchronous operation not supported.
    DCHECK(callback);
    write_callback_ = callback;
  } else {
    LogConnectCompletion(rv);
  }

  return rv;
}

int TCPClientSocketLibevent::DoConnectLoop(int result) {
  DCHECK_NE(next_connect_state_, CONNECT_STATE_NONE);

  int rv = result;
  do {
    ConnectState state = next_connect_state_;
    next_connect_state_ = CONNECT_STATE_NONE;
    switch (state) {
      case CONNECT_STATE_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        LOG(DFATAL) << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_connect_state_ != CONNECT_STATE_NONE);

  return rv;
}

int TCPClientSocketLibevent::DoConnect() {
  DCHECK(current_ai_);

  DCHECK_EQ(0, connect_os_error_);

  net_log_.BeginEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT,
                      new NetLogStringParameter(
                          "address", NetAddressToStringWithPort(current_ai_)));

  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;

  // Create a non-blocking socket.
  connect_os_error_ = CreateSocket(current_ai_);
  if (connect_os_error_)
    return MapPosixError(connect_os_error_);

  // Connect the socket.
  if (!HANDLE_EINTR(connect(socket_, current_ai_->ai_addr,
                            static_cast<int>(current_ai_->ai_addrlen)))) {
    // Connected without waiting!
    return OK;
  }

  // Check if the connect() failed synchronously.
  connect_os_error_ = errno;
  if (connect_os_error_ != EINPROGRESS)
    return MapConnectError(connect_os_error_);

  // Otherwise the connect() is going to complete asynchronously, so watch
  // for its completion.
  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE, &write_socket_watcher_,
          &write_watcher_)) {
    connect_os_error_ = errno;
    DLOG(INFO) << "WatchFileDescriptor failed: " << connect_os_error_;
    return MapPosixError(connect_os_error_);
  }

  return ERR_IO_PENDING;
}

int TCPClientSocketLibevent::DoConnectComplete(int result) {
  // Log the end of this attempt (and any OS error it threw).
  int os_error = connect_os_error_;
  connect_os_error_ = 0;
  scoped_refptr<NetLog::EventParameters> params;
  if (result != OK)
    params = new NetLogIntegerParameter("os_error", os_error);
  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT_ATTEMPT, params);

  write_socket_watcher_.StopWatchingFileDescriptor();

  if (result == OK) {
    use_history_.set_was_ever_connected();
    return OK;  // Done!
  }

  // Close whatever partially connected socket we currently have.
  DoDisconnect();

  // Try to fall back to the next address in the list.
  if (current_ai_->ai_next) {
    next_connect_state_ = CONNECT_STATE_CONNECT;
    current_ai_ = current_ai_->ai_next;
    return OK;
  }

  // Otherwise there is nothing to fall back to, so give up.
  return result;
}

void TCPClientSocketLibevent::Disconnect() {
  DCHECK(CalledOnValidThread());

  DoDisconnect();
  current_ai_ = NULL;
}

void TCPClientSocketLibevent::DoDisconnect() {
  if (socket_ == kInvalidSocket)
    return;

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  if (HANDLE_EINTR(close(socket_)) < 0)
    PLOG(ERROR) << "close";
  socket_ = kInvalidSocket;
}

bool TCPClientSocketLibevent::IsConnected() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == kInvalidSocket || waiting_connect())
    return false;

  // Check if connection is alive.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv == 0)
    return false;
  if (rv == -1 && errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

bool TCPClientSocketLibevent::IsConnectedAndIdle() const {
  DCHECK(CalledOnValidThread());

  if (socket_ == kInvalidSocket || waiting_connect())
    return false;

  // Check if connection is alive and we haven't received any data
  // unexpectedly.
  char c;
  int rv = HANDLE_EINTR(recv(socket_, &c, 1, MSG_PEEK));
  if (rv >= 0)
    return false;
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return false;

  return true;
}

int TCPClientSocketLibevent::Read(IOBuffer* buf,
                                  int buf_len,
                                  CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect());
  DCHECK(!read_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  int nread = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  if (nread >= 0) {
    static StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(nread);
    if (nread > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED,
                      new NetLogIntegerParameter("num_bytes", nread));
    return nread;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    DLOG(INFO) << "read failed, errno " << errno;
    return MapPosixError(errno);
  }

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_READ,
          &read_socket_watcher_, &read_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed on read, errno " << errno;
    return MapPosixError(errno);
  }

  read_buf_ = buf;
  read_buf_len_ = buf_len;
  read_callback_ = callback;
  return ERR_IO_PENDING;
}

int TCPClientSocketLibevent::Write(IOBuffer* buf,
                                   int buf_len,
                                   CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect());
  DCHECK(!write_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  int nwrite = HANDLE_EINTR(write(socket_, buf->data(), buf_len));
  if (nwrite >= 0) {
    static StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(nwrite);
    if (nwrite > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddEvent(NetLog::TYPE_SOCKET_BYTES_SENT,
                      new NetLogIntegerParameter("num_bytes", nwrite));
    return nwrite;
  }
  if (errno != EAGAIN && errno != EWOULDBLOCK)
    return MapPosixError(errno);

  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE,
          &write_socket_watcher_, &write_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed on write, errno " << errno;
    return MapPosixError(errno);
  }

  write_buf_ = buf;
  write_buf_len_ = buf_len;
  write_callback_ = callback;
  return ERR_IO_PENDING;
}

bool TCPClientSocketLibevent::SetReceiveBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
      reinterpret_cast<const char*>(&size),
      sizeof(size));
  DCHECK(!rv) << "Could not set socket receive buffer size: " << errno;
  return rv == 0;
}

bool TCPClientSocketLibevent::SetSendBufferSize(int32 size) {
  DCHECK(CalledOnValidThread());
  int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
      reinterpret_cast<const char*>(&size),
      sizeof(size));
  DCHECK(!rv) << "Could not set socket send buffer size: " << errno;
  return rv == 0;
}


int TCPClientSocketLibevent::CreateSocket(const addrinfo* ai) {
  socket_ = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (socket_ == kInvalidSocket)
    return errno;

  if (SetNonBlocking(socket_)) {
    const int err = errno;
    close(socket_);
    socket_ = kInvalidSocket;
    return err;
  }

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_client_socket_win.cc after searching for "NODELAY".
  DisableNagle(socket_);  // If DisableNagle fails, we don't care.

  return 0;
}

void TCPClientSocketLibevent::LogConnectCompletion(int net_error) {
  scoped_refptr<NetLog::EventParameters> params;
  if (net_error != OK)
    params = new NetLogIntegerParameter("net_error", net_error);
  net_log_.EndEvent(NetLog::TYPE_TCP_CONNECT, params);
}

void TCPClientSocketLibevent::DoReadCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(read_callback_);

  // since Run may result in Read being called, clear read_callback_ up front.
  CompletionCallback* c = read_callback_;
  read_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketLibevent::DoWriteCallback(int rv) {
  DCHECK_NE(rv, ERR_IO_PENDING);
  DCHECK(write_callback_);

  // since Run may result in Write being called, clear write_callback_ up front.
  CompletionCallback* c = write_callback_;
  write_callback_ = NULL;
  c->Run(rv);
}

void TCPClientSocketLibevent::DidCompleteConnect() {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);

  // Get the error that connect() completed with.
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &os_error, &len) < 0)
    os_error = errno;

  // TODO(eroman): Is this check really necessary?
  if (os_error == EINPROGRESS || os_error == EALREADY) {
    NOTREACHED();  // This indicates a bug in libevent or our code.
    return;
  }

  connect_os_error_ = os_error;
  int rv = DoConnectLoop(MapConnectError(os_error));
  if (rv != ERR_IO_PENDING) {
    LogConnectCompletion(rv);
    DoWriteCallback(rv);
  }
}

void TCPClientSocketLibevent::DidCompleteRead() {
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(read(socket_, read_buf_->data(),
                                        read_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    static StatsCounter read_bytes("tcp.read_bytes");
    read_bytes.Add(bytes_transferred);
    if (bytes_transferred > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddEvent(NetLog::TYPE_SOCKET_BYTES_RECEIVED,
                      new NetLogIntegerParameter("num_bytes", result));
  } else {
    result = MapPosixError(errno);
  }

  if (result != ERR_IO_PENDING) {
    read_buf_ = NULL;
    read_buf_len_ = 0;
    bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    DoReadCallback(result);
  }
}

void TCPClientSocketLibevent::DidCompleteWrite() {
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(write(socket_, write_buf_->data(),
                                         write_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    result = bytes_transferred;
    static StatsCounter write_bytes("tcp.write_bytes");
    write_bytes.Add(bytes_transferred);
    if (bytes_transferred > 0)
      use_history_.set_was_used_to_convey_data();
    net_log_.AddEvent(NetLog::TYPE_SOCKET_BYTES_SENT,
                      new NetLogIntegerParameter("num_bytes", result));
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

int TCPClientSocketLibevent::GetPeerAddress(AddressList* address) const {
  DCHECK(CalledOnValidThread());
  DCHECK(address);
  if (!IsConnected())
    return ERR_UNEXPECTED;
  address->Copy(current_ai_, false);
  return OK;
}

void TCPClientSocketLibevent::SetSubresourceSpeculation() {
  use_history_.set_subresource_speculation();
}

void TCPClientSocketLibevent::SetOmniboxSpeculation() {
  use_history_.set_omnibox_speculation();
}

bool TCPClientSocketLibevent::WasEverUsed() const {
  return use_history_.was_used_to_convey_data();
}

}  // namespace net
