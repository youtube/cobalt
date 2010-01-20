// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket_libevent.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "base/eintr_wrapper.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/trace_event.h"
#include "net/base/io_buffer.h"
#include "net/base/load_log.h"
#include "net/base/net_errors.h"
#if defined(USE_SYSTEM_LIBEVENT)
#include <event.h>
#else
#include "third_party/libevent/event.h"
#endif

namespace net {

namespace {

const int kInvalidSocket = -1;

// Return 0 on success, -1 on failure.
// Too small a function to bother putting in a library?
int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (-1 == flags)
    return flags;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

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

// Given os_error, an errno from a connect() attempt, returns true if
// connect() should be retried with another address.
bool ShouldTryNextAddress(int os_error) {
  switch (os_error) {
    case EADDRNOTAVAIL:
    case EAFNOSUPPORT:
    case ECONNREFUSED:
    case ECONNRESET:
    case EACCES:
    case EPERM:
    case ENETUNREACH:
    case EHOSTUNREACH:
    case ENETDOWN:
    case ETIMEDOUT:
      return true;
    default:
      return false;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TCPClientSocketLibevent::TCPClientSocketLibevent(const AddressList& addresses)
    : socket_(kInvalidSocket),
      addresses_(addresses),
      current_ai_(addresses_.head()),
      waiting_connect_(false),
      read_watcher_(this),
      write_watcher_(this),
      read_callback_(NULL),
      write_callback_(NULL) {
}

TCPClientSocketLibevent::~TCPClientSocketLibevent() {
  Disconnect();
}

int TCPClientSocketLibevent::Connect(CompletionCallback* callback,
                                     LoadLog* load_log) {
  // If already connected, then just return OK.
  if (socket_ != kInvalidSocket)
    return OK;

  DCHECK(!waiting_connect_);
  DCHECK(!load_log_);

  TRACE_EVENT_BEGIN("socket.connect", this, "");

  LoadLog::BeginEvent(load_log, LoadLog::TYPE_TCP_CONNECT);

  int rv = DoConnect();

  if (rv == ERR_IO_PENDING) {
    // Synchronous operation not supported.
    DCHECK(callback);

    load_log_ = load_log;
    waiting_connect_ = true;
    write_callback_ = callback;
  } else {
    TRACE_EVENT_END("socket.connect", this, "");
    LoadLog::EndEvent(load_log, LoadLog::TYPE_TCP_CONNECT);
  }

  return rv;
}

int TCPClientSocketLibevent::DoConnect() {
  while (true) {
    DCHECK(current_ai_);

    int rv = CreateSocket(current_ai_);
    if (rv != OK)
      return rv;

    if (!HANDLE_EINTR(connect(socket_, current_ai_->ai_addr,
                              static_cast<int>(current_ai_->ai_addrlen)))) {
      // Connected without waiting!
      return OK;
    }

    int os_error = errno;
    if (os_error == EINPROGRESS)
      break;

    close(socket_);
    socket_ = kInvalidSocket;

    if (current_ai_->ai_next && ShouldTryNextAddress(os_error)) {
      // connect() can fail synchronously for an address even on a
      // non-blocking socket.  As an example, this can happen when there is
      // no route to the host.  Retry using the next address in the list.
      current_ai_ = current_ai_->ai_next;
    } else {
      DLOG(INFO) << "connect failed: " << os_error;
      return MapConnectError(os_error);
    }
  }

  // Initialize write_socket_watcher_ and link it to our MessagePump.
  // POLLOUT is set if the connection is established.
  // POLLIN is set if the connection fails.
  if (!MessageLoopForIO::current()->WatchFileDescriptor(
          socket_, true, MessageLoopForIO::WATCH_WRITE, &write_socket_watcher_,
          &write_watcher_)) {
    DLOG(INFO) << "WatchFileDescriptor failed: " << errno;
    close(socket_);
    socket_ = kInvalidSocket;
    return MapPosixError(errno);
  }

  return ERR_IO_PENDING;
}

void TCPClientSocketLibevent::Disconnect() {
  if (socket_ == kInvalidSocket)
    return;

  TRACE_EVENT_INSTANT("socket.disconnect", this, "");

  bool ok = read_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  ok = write_socket_watcher_.StopWatchingFileDescriptor();
  DCHECK(ok);
  close(socket_);
  socket_ = kInvalidSocket;
  waiting_connect_ = false;

  // Reset for next time.
  current_ai_ = addresses_.head();
}

bool TCPClientSocketLibevent::IsConnected() const {
  if (socket_ == kInvalidSocket || waiting_connect_)
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
  if (socket_ == kInvalidSocket || waiting_connect_)
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
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(!read_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  TRACE_EVENT_BEGIN("socket.read", this, "");
  int nread = HANDLE_EINTR(read(socket_, buf->data(), buf_len));
  if (nread >= 0) {
    TRACE_EVENT_END("socket.read", this, StringPrintf("%d bytes", nread));
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
  DCHECK_NE(kInvalidSocket, socket_);
  DCHECK(!waiting_connect_);
  DCHECK(!write_callback_);
  // Synchronous operation not supported
  DCHECK(callback);
  DCHECK_GT(buf_len, 0);

  TRACE_EVENT_BEGIN("socket.write", this, "");
  int nwrite = HANDLE_EINTR(write(socket_, buf->data(), buf_len));
  if (nwrite >= 0) {
    TRACE_EVENT_END("socket.write", this, StringPrintf("%d bytes", nwrite));
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
  int rv = setsockopt(socket_, SOL_SOCKET, SO_RCVBUF,
      reinterpret_cast<const char*>(&size),
      sizeof(size));
  DCHECK(!rv) << "Could not set socket receive buffer size: " << errno;
  return rv == 0;
}

bool TCPClientSocketLibevent::SetSendBufferSize(int32 size) {
  int rv = setsockopt(socket_, SOL_SOCKET, SO_SNDBUF,
      reinterpret_cast<const char*>(&size),
      sizeof(size));
  DCHECK(!rv) << "Could not set socket send buffer size: " << errno;
  return rv == 0;
}


int TCPClientSocketLibevent::CreateSocket(const addrinfo* ai) {
  socket_ = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (socket_ == kInvalidSocket)
    return MapPosixError(errno);

  if (SetNonBlocking(socket_)) {
    const int err = MapPosixError(errno);
    close(socket_);
    socket_ = kInvalidSocket;
    return err;
  }

  // This mirrors the behaviour on Windows. See the comment in
  // tcp_client_socket_win.cc after searching for "NODELAY".
  DisableNagle(socket_);  // If DisableNagle fails, we don't care.

  return OK;
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
  int result = ERR_UNEXPECTED;

  // Check to see if connect succeeded
  int os_error = 0;
  socklen_t len = sizeof(os_error);
  if (getsockopt(socket_, SOL_SOCKET, SO_ERROR, &os_error, &len) < 0)
    os_error = errno;

  if (os_error == EINPROGRESS || os_error == EALREADY) {
    NOTREACHED();  // This indicates a bug in libevent or our code.
    result = ERR_IO_PENDING;
  } else if (current_ai_->ai_next && ShouldTryNextAddress(os_error)) {
    // This address failed, try next one in list.
    const addrinfo* next = current_ai_->ai_next;
    Disconnect();
    current_ai_ = next;
    scoped_refptr<LoadLog> load_log;
    load_log.swap(load_log_);
    TRACE_EVENT_END("socket.connect", this, "");
    LoadLog::EndEvent(load_log, LoadLog::TYPE_TCP_CONNECT);
    result = Connect(write_callback_, load_log);
  } else {
    result = MapConnectError(os_error);
    bool ok = write_socket_watcher_.StopWatchingFileDescriptor();
    DCHECK(ok);
    waiting_connect_ = false;
    TRACE_EVENT_END("socket.connect", this, "");
    LoadLog::EndEvent(load_log_, LoadLog::TYPE_TCP_CONNECT);
    load_log_ = NULL;
  }

  if (result != ERR_IO_PENDING) {
    DoWriteCallback(result);
  }
}

void TCPClientSocketLibevent::DidCompleteRead() {
  int bytes_transferred;
  bytes_transferred = HANDLE_EINTR(read(socket_, read_buf_->data(),
                                        read_buf_len_));

  int result;
  if (bytes_transferred >= 0) {
    TRACE_EVENT_END("socket.read", this,
                    StringPrintf("%d bytes", bytes_transferred));
    result = bytes_transferred;
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
    TRACE_EVENT_END("socket.write", this,
                    StringPrintf("%d bytes", bytes_transferred));
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

int TCPClientSocketLibevent::GetPeerName(struct sockaddr* name,
                                         socklen_t* namelen) {
  return ::getpeername(socket_, name, namelen);
}

}  // namespace net
