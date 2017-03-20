// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/stream_listen_socket.h"

#if defined(OS_WIN)
// winsock2.h must be included first in order to ensure it is included before
// windows.h.
#include <winsock2.h>
#elif defined(OS_POSIX)
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net/base/net_errors.h"
#elif defined(OS_STARBOARD)
#include "starboard/socket.h"
#endif

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

using std::string;

#if defined(OS_WIN)
typedef int socklen_t;
#endif  // defined(OS_WIN)

namespace net {

namespace {

const int kReadBufSize = 4096;
#if defined(__LB_PS4__) || defined(__LB_LINUX__)
// Don't send SIGPIPE when the other end disconnects.
// We will still receive an EPIPE as expected.
const int kDefaultMsgFlags = MSG_NOSIGNAL;
#else
const int kDefaultMsgFlags = 0;
#endif

}  // namespace

#if defined(OS_WIN)
const SocketDescriptor StreamListenSocket::kInvalidSocket = INVALID_SOCKET;
const int StreamListenSocket::kSocketError = SOCKET_ERROR;
#elif defined(OS_POSIX)
const SocketDescriptor StreamListenSocket::kInvalidSocket = -1;
const int StreamListenSocket::kSocketError = -1;
#elif defined(OS_STARBOARD)
const SocketDescriptor StreamListenSocket::kInvalidSocket = kSbSocketInvalid;
const int StreamListenSocket::kSocketError = -1;
#endif

StreamListenSocket::StreamListenSocket(SocketDescriptor s,
                                       StreamListenSocket::Delegate* del)
    : socket_delegate_(del),
      socket_(s),
      reads_paused_(false),
      has_pending_reads_(false) {
#if defined(OS_WIN)
  socket_event_ = WSACreateEvent();
  // TODO(ibrar): error handling in case of socket_event_ == WSA_INVALID_EVENT.
  WatchSocket(NOT_WAITING);
#elif defined(OS_POSIX)
  wait_state_ = NOT_WAITING;
#elif defined(OS_STARBOARD)
  wait_state_ = NOT_WAITING;
#endif
}

StreamListenSocket::~StreamListenSocket() {
#if defined(OS_WIN)
  if (socket_event_) {
    WSACloseEvent(socket_event_);
    socket_event_ = WSA_INVALID_EVENT;
  }
#endif
  CloseSocket(socket_);
}

void StreamListenSocket::Send(const char* bytes, int len,
                              bool append_linefeed) {
  SendInternal(bytes, len);
  if (append_linefeed)
    SendInternal("\r\n", 2);
}

void StreamListenSocket::Send(const string& str, bool append_linefeed) {
  Send(str.data(), static_cast<int>(str.length()), append_linefeed);
}

int StreamListenSocket::GetLocalAddress(IPEndPoint* address) {
#if defined(OS_STARBOARD)
  SbSocketAddress sb_address;
  if (!SbSocketGetLocalAddress(socket_, &sb_address)) {
    return ERR_FAILED;
  }
  if (!address->FromSbSocketAddress(&sb_address)) {
    return ERR_FAILED;
  }
  return OK;
#else  // defined(OS_STARBOARD)
  SockaddrStorage storage;
  if (getsockname(socket_, storage.addr, &storage.addr_len)) {
#if defined(OS_WIN)
    int err = WSAGetLastError();
#else
    int err = errno;
#endif
    return MapSystemError(err);
  }
  if (!address->FromSockAddr(storage.addr, storage.addr_len))
    return ERR_FAILED;
  return OK;
#endif  // defined(OS_STARBOARD)
}

SocketDescriptor StreamListenSocket::AcceptSocket() {
#if defined(OS_STARBOARD)
  SocketDescriptor conn = SbSocketAccept(socket_);
#else
  SocketDescriptor conn = HANDLE_EINTR(accept(socket_, NULL, NULL));
#endif
  if (conn == kInvalidSocket)
    LOG(ERROR) << "Error accepting connection.";
  else
    SetNonBlocking(conn);
  return conn;
}

void StreamListenSocket::SendInternal(const char* bytes, int len) {
  char* send_buf = const_cast<char *>(bytes);
  int len_left = len;
  while (true) {
#if defined(OS_STARBOARD)
    int sent = SbSocketSendTo(socket_, send_buf, len_left, NULL);
#else
    int sent = HANDLE_EINTR(
        send(socket_, send_buf, len_left, kDefaultMsgFlags));
#endif
    if (sent == len_left) {  // A shortcut to avoid extraneous checks.
      break;
    }

    // __LB_SHELL__: In some consoles |sent| might return the actual error code,
    // and so can be even lower than the native |kSocketError|.
    COMPILE_ASSERT(kSocketError < 0, native_socket_error_should_be_negative);
    if (sent <= kSocketError) {
#if defined(OS_WIN)
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        LOG(ERROR) << "send failed: WSAGetLastError()==" << WSAGetLastError();
#elif defined(OS_STARBOARD)
      if (SbSocketGetLastError(socket_) != kSbSocketPending) {
        LOG(ERROR) << "SbSocketSendTo failed: error = "
                   << SbSocketGetLastError(socket_);
#elif defined(__LB_SHELL__)
      if (!lb_net_would_block()) {
        LOG(ERROR) << "send failed: errno==" << lb_net_errno();
#elif defined(OS_POSIX)
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG(ERROR) << "send failed: errno==" << errno;
#endif
        break;
      }
      // Otherwise we would block, and now we have to wait for a retry.
      // Fall through to PlatformThread::YieldCurrentThread()
    } else {
      // sent != len_left according to the shortcut above.
      // Shift the buffer start and send the remainder after a short while.
      send_buf += sent;
      len_left -= sent;
    }
    base::PlatformThread::YieldCurrentThread();
  }
}

void StreamListenSocket::Listen() {
#if defined(OS_STARBOARD)
  SbSocketListen(socket_);
  WatchSocket(WAITING_ACCEPT);
#else  // defined(OS_STARBOARD)
  int backlog = 10;  // TODO(erikkay): maybe don't allow any backlog?
  if (listen(socket_, backlog) == -1) {
    // TODO(erikkay): error handling.
    LOG(ERROR) << "Could not listen on socket.";
    return;
  }
#if defined(OS_POSIX)
  WatchSocket(WAITING_ACCEPT);
#endif
#endif  // defined(OS_STARBOARD)
}

void StreamListenSocket::Read() {
  char buf[kReadBufSize + 1];  // +1 for null termination.
#if defined(OS_STARBOARD)
  int len;
  do {
    len = SbSocketReceiveFrom(socket_, buf, kReadBufSize, NULL);
    if (len < 0) {
      SbSocketError error = SbSocketGetLastError(socket_);
      break;
    }
    if (len == 0) {
      Close();
    } else {
      DCHECK_LT(0, len);
      DCHECK_GE(kReadBufSize, len);
      buf[len] = 0;  // We already create a buffer with +1 length.
      socket_delegate_->DidRead(this, buf, len);
    }
  } while (len == kReadBufSize);
#else  // defined(OS_STARBOARD)
  int len;
  do {
    len = HANDLE_EINTR(recv(socket_, buf, kReadBufSize, kDefaultMsgFlags));
    if (len == kSocketError) {
#if defined(OS_WIN)
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
        break;
      } else {
        // TODO(ibrar): some error handling required here.
        break;
      }
    } else if (len == 0) {
      // In Windows, Close() is called by OnObjectSignaled. In POSIX, we need
      // to call it here.
#if defined(OS_POSIX)
      Close();
#endif
    } else {
#if defined(__LB_SHELL__)
      if (len < 0) {
        // Some platforms return a negative error code for length instead of a
        // simple -1.  Thus, we must check it in the len < 0 case.
        if (!lb_net_would_block()) {
          Close();
        }
        break;
      }
#endif
      // TODO(ibrar): maybe change DidRead to take a length instead.
      DCHECK_GT(len, 0);
      DCHECK_LE(len, kReadBufSize);
      buf[len] = 0;  // Already create a buffer with +1 length.
      socket_delegate_->DidRead(this, buf, len);
    }
  } while (len == kReadBufSize);
#endif  // defined(OS_STARBOARD)
}

void StreamListenSocket::Close() {
#if defined(OS_POSIX) || defined(OS_STARBOARD)
  if (wait_state_ == NOT_WAITING)
    return;
  wait_state_ = NOT_WAITING;
#endif
  UnwatchSocket();
  socket_delegate_->DidClose(this);
}

void StreamListenSocket::CloseSocket(SocketDescriptor s) {
  if (s && s != kInvalidSocket) {
    UnwatchSocket();
#if defined(OS_WIN)
    closesocket(s);
#elif defined(OS_STARBOARD)
    SbSocketDestroy(s);
#elif defined(__LB_SHELL__)
    lb_close_socket(s);
#elif defined(OS_POSIX)
    close(s);
#endif
  }
}

void StreamListenSocket::WatchSocket(WaitState state) {
#if defined(OS_WIN)
  WSAEventSelect(socket_, socket_event_, FD_ACCEPT | FD_CLOSE | FD_READ);
  watcher_.StartWatching(socket_event_, this);
#elif defined(OS_STARBOARD)
  MessageLoopForIO::current()->Watch(
      socket_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  wait_state_ = state;
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
  watcher_.StartWatching(socket_, base::MessagePumpShell::WATCH_READ, this);
  wait_state_ = state;
#elif defined(OS_POSIX)
  // Implicitly calls StartWatchingFileDescriptor().
  MessageLoopForIO::current()->WatchFileDescriptor(
      socket_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  wait_state_ = state;
#endif
}

void StreamListenSocket::UnwatchSocket() {
#if defined(OS_WIN)
  watcher_.StopWatching();
#elif defined(OS_STARBOARD)
  watcher_.StopWatchingSocket();
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
  watcher_.StopWatching();
#elif defined(OS_POSIX)
  watcher_.StopWatchingFileDescriptor();
#endif
}

// TODO(ibrar): We can add these functions into OS dependent files.
#if defined(OS_WIN)
// MessageLoop watcher callback.
void StreamListenSocket::OnObjectSignaled(HANDLE object) {
  WSANETWORKEVENTS ev;
  if (kSocketError == WSAEnumNetworkEvents(socket_, socket_event_, &ev)) {
    // TODO
    return;
  }

  // The object was reset by WSAEnumNetworkEvents.  Watch for the next signal.
  watcher_.StartWatching(object, this);

  if (ev.lNetworkEvents == 0) {
    // Occasionally the event is set even though there is no new data.
    // The net seems to think that this is ignorable.
    return;
  }
  if (ev.lNetworkEvents & FD_ACCEPT) {
    Accept();
  }
  if (ev.lNetworkEvents & FD_READ) {
    if (reads_paused_) {
      has_pending_reads_ = true;
    } else {
      Read();
    }
  }
  if (ev.lNetworkEvents & FD_CLOSE) {
    Close();
  }
}
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
// MessageLoop watcher callback.
void StreamListenSocket::OnObjectSignaled(int object) {

  // Object Watcher removes the object whenever it gets signaled, so rewatch it
  watcher_.StartWatching(object, base::MessagePumpShell::WATCH_READ, this);

  switch (wait_state_) {
    case WAITING_ACCEPT:
      Accept();
      break;
    case WAITING_READ:
      if (reads_paused_) {
        has_pending_reads_ = true;
      } else {
        Read();
      }
      break;
    default:
      // Close() is called by Read() in the Linux case.
      NOTREACHED();
      break;
  }
}
#elif defined(OS_STARBOARD)
void StreamListenSocket::OnSocketReadyToRead(SbSocket socket) {
  switch (wait_state_) {
    case WAITING_ACCEPT:
      Accept();
      break;
    case WAITING_READ:
      if (reads_paused_) {
        has_pending_reads_ = true;
      } else {
        Read();
      }
      break;
    default:
      NOTREACHED();
      break;
  }
}

void StreamListenSocket::OnSocketReadyToWrite(SbSocket socket) {
  // SbSocketWaiter callback, we don't listen for write events so we shouldn't
  // ever reach here.
  NOTREACHED();
}
#elif defined(OS_POSIX)
void StreamListenSocket::OnFileCanReadWithoutBlocking(int fd) {
  switch (wait_state_) {
    case WAITING_ACCEPT:
      Accept();
      break;
    case WAITING_READ:
      if (reads_paused_) {
        has_pending_reads_ = true;
      } else {
        Read();
      }
      break;
    default:
      // Close() is called by Read() in the Linux case.
      NOTREACHED();
      break;
  }
}

void StreamListenSocket::OnFileCanWriteWithoutBlocking(int fd) {
  // MessagePumpLibevent callback, we don't listen for write events
  // so we shouldn't ever reach here.
  NOTREACHED();
}

#endif

void StreamListenSocket::PauseReads() {
  DCHECK(!reads_paused_);
  reads_paused_ = true;
}

void StreamListenSocket::ResumeReads() {
  DCHECK(reads_paused_);
  reads_paused_ = false;
  if (has_pending_reads_) {
    has_pending_reads_ = false;
    Read();
  }
}

}  // namespace net
