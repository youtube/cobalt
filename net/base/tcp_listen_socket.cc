// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

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
#endif

#include "base/bind.h"
#include "base/eintr_wrapper.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "net/base/net_util.h"
#include "net/base/tcp_listen_socket.h"

#if defined(OS_WIN)
typedef int socklen_t;
#include "net/base/winsock_init.h"
#endif  // defined(OS_WIN)

namespace net {

namespace {

const int kReadBufSize = 4096;
const int kMaxSendBufSize = 1024 * 1024 * 5;  // 5MB

const net::BackoffEntry::Policy kSendBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  0,

  // Initial delay for exponential back-off in ms.
  25,

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0,

  // Maximum amount of time we are willing to delay our request in ms.
  100,

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

}  // namespace

#if defined(OS_WIN)
const SOCKET TCPListenSocket::kInvalidSocket = INVALID_SOCKET;
const int TCPListenSocket::kSocketError = SOCKET_ERROR;
#elif defined(OS_POSIX)
const SOCKET TCPListenSocket::kInvalidSocket = -1;
const int TCPListenSocket::kSocketError = -1;
#endif

TCPListenSocket* TCPListenSocket::CreateAndListen(
    const std::string& ip, int port, ListenSocket::ListenSocketDelegate *del) {
  SOCKET s = CreateAndBind(ip, port);
  if (s == kInvalidSocket) {
    // TODO(erikkay): error handling
  } else {
    TCPListenSocket* sock = new TCPListenSocket(s, del);
    sock->Listen();
    return sock;
  }
  return NULL;
}

void TCPListenSocket::PauseReads() {
  DCHECK(!reads_paused_);
  reads_paused_ = true;
}

void TCPListenSocket::ResumeReads() {
  DCHECK(reads_paused_);
  reads_paused_ = false;
  if (has_pending_reads_) {
    has_pending_reads_ = false;
    Read();
  }
}

TCPListenSocket::TCPListenSocket(SOCKET s,
                                 ListenSocket::ListenSocketDelegate *del)
    : ListenSocket(del),
      socket_(s),
      reads_paused_(false),
      has_pending_reads_(false),
      send_pending_size_(0),
      send_error_(false),
      send_backoff_(&kSendBackoffPolicy) {
#if defined(OS_WIN)
  socket_event_ = WSACreateEvent();
  // TODO(ibrar): error handling in case of socket_event_ == WSA_INVALID_EVENT
  WatchSocket(NOT_WAITING);
#elif defined(OS_POSIX)
  wait_state_ = NOT_WAITING;
#endif
}

TCPListenSocket::~TCPListenSocket() {
#if defined(OS_WIN)
  if (socket_event_) {
    WSACloseEvent(socket_event_);
    socket_event_ = WSA_INVALID_EVENT;
  }
#endif
  CloseSocket(socket_);
}

SOCKET TCPListenSocket::CreateAndBind(const std::string& ip, int port) {
#if defined(OS_WIN)
  EnsureWinsockInit();
#endif

  SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (s != kInvalidSocket) {
#if defined(OS_POSIX)
    // Allow rapid reuse.
    static const int kOn = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &kOn, sizeof(kOn));
#endif
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip.c_str());
    addr.sin_port = base::HostToNet16(port);
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr))) {
#if defined(OS_WIN)
      closesocket(s);
#elif defined(OS_POSIX)
      close(s);
#endif
      s = kInvalidSocket;
    }
  }
  return s;
}

SOCKET TCPListenSocket::Accept(SOCKET s) {
  sockaddr_in from;
  socklen_t from_len = sizeof(from);
  SOCKET conn =
      HANDLE_EINTR(accept(s, reinterpret_cast<sockaddr*>(&from), &from_len));
  if (conn != kInvalidSocket) {
    SetNonBlocking(conn);
  }
  return conn;
}

void TCPListenSocket::SendInternal(const char* bytes, int len) {
  DCHECK(bytes);
  if (!bytes || len <= 0)
    return;

  if (send_error_)
    return;

  if (send_pending_size_ + len > kMaxSendBufSize) {
    LOG(ERROR) << "send failed: buffer overrun";
    send_buffers_.clear();
    send_pending_size_ = 0;
    send_error_ = true;
    return;
  }

  scoped_refptr<IOBuffer> buffer(new IOBuffer(len));
  memcpy(buffer->data(), bytes, len);
  send_buffers_.push_back(new DrainableIOBuffer(buffer, len));
  send_pending_size_ += len;

  if (!send_timer_.IsRunning())
    SendData();
}

void TCPListenSocket::Listen() {
  int backlog = 10;  // TODO(erikkay): maybe don't allow any backlog?
  listen(socket_, backlog);
  // TODO(erikkay): error handling
#if defined(OS_POSIX)
  WatchSocket(WAITING_ACCEPT);
#endif
}

void TCPListenSocket::Accept() {
  SOCKET conn = Accept(socket_);
  if (conn != kInvalidSocket) {
    scoped_refptr<TCPListenSocket> sock(
        new TCPListenSocket(conn, socket_delegate_));
    // it's up to the delegate to AddRef if it wants to keep it around
#if defined(OS_POSIX)
    sock->WatchSocket(WAITING_READ);
#endif
    socket_delegate_->DidAccept(this, sock);
  } else {
    // TODO(ibrar): some error handling required here
  }
}

void TCPListenSocket::Read() {
  char buf[kReadBufSize + 1];  // +1 for null termination
  int len;
  do {
    len = HANDLE_EINTR(recv(socket_, buf, kReadBufSize, 0));
    if (len == kSocketError) {
#if defined(OS_WIN)
      int err = WSAGetLastError();
      if (err == WSAEWOULDBLOCK) {
#elif defined(OS_POSIX)
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
#endif
        break;
      } else {
        // TODO(ibrar): some error handling required here
        break;
      }
    } else if (len == 0) {
      // In Windows, Close() is called by OnObjectSignaled.  In POSIX, we need
      // to call it here.
#if defined(OS_POSIX)
      Close();
#endif
    } else {
      // TODO(ibrar): maybe change DidRead to take a length instead
      DCHECK_GT(len, 0);
      DCHECK_LE(len, kReadBufSize);
      buf[len] = 0;  // already create a buffer with +1 length
      socket_delegate_->DidRead(this, buf, len);
    }
  } while (len == kReadBufSize);
}

void TCPListenSocket::Close() {
#if defined(OS_POSIX)
  if (wait_state_ == NOT_WAITING)
    return;
  wait_state_ = NOT_WAITING;
#endif
  UnwatchSocket();
  socket_delegate_->DidClose(this);
}

void TCPListenSocket::CloseSocket(SOCKET s) {
  if (s && s != kInvalidSocket) {
    UnwatchSocket();
#if defined(OS_WIN)
    closesocket(s);
#elif defined(OS_POSIX)
    close(s);
#endif
  }
}

void TCPListenSocket::WatchSocket(WaitState state) {
#if defined(OS_WIN)
  WSAEventSelect(socket_, socket_event_, FD_ACCEPT | FD_CLOSE | FD_READ);
  watcher_.StartWatching(socket_event_, this);
#elif defined(OS_POSIX)
  // Implicitly calls StartWatchingFileDescriptor().
  MessageLoopForIO::current()->WatchFileDescriptor(
      socket_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
  wait_state_ = state;
#endif
}

void TCPListenSocket::UnwatchSocket() {
#if defined(OS_WIN)
  watcher_.StopWatching();
#elif defined(OS_POSIX)
  watcher_.StopWatchingFileDescriptor();
#endif
}

// TODO(ibrar): We can add these functions into OS dependent files
#if defined(OS_WIN)
// MessageLoop watcher callback
void TCPListenSocket::OnObjectSignaled(HANDLE object) {
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
#elif defined(OS_POSIX)
void TCPListenSocket::OnFileCanReadWithoutBlocking(int fd) {
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

void TCPListenSocket::OnFileCanWriteWithoutBlocking(int fd) {
  // MessagePumpLibevent callback, we don't listen for write events
  // so we shouldn't ever reach here.
  NOTREACHED();
}

#endif

void TCPListenSocket::SendData() {
  DCHECK(!send_buffers_.empty());

  int total_sent = 0;

  // Send data until all buffers have been sent or a call would block.
  while (!send_buffers_.empty()) {
    scoped_refptr<DrainableIOBuffer> buffer = send_buffers_.front();

    int len_left = buffer->BytesRemaining();
    int sent = HANDLE_EINTR(send(socket_, buffer->data(), len_left, 0));
    if (sent > 0) {
      if (sent == len_left)
        send_buffers_.pop_front();
      else
        buffer->DidConsume(sent);

      total_sent += sent;
    } else if (sent == kSocketError) {
#if defined(OS_WIN)
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        LOG(ERROR) << "send failed: WSAGetLastError()==" << WSAGetLastError();
#elif defined(OS_POSIX)
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        LOG(ERROR) << "send failed: errno==" << errno;
#endif
        // Don't try to re-send data after a socket error.
        send_buffers_.clear();
        send_pending_size_ = 0;
        send_error_ = true;
        return;
      }

      // The call would block. Don't send any more data at this time.
      break;
    } else {
      NOTREACHED();
      break;
    }
  }

  if (total_sent > 0) {
    send_pending_size_ -= total_sent;
    DCHECK_GE(send_pending_size_, 0);

    // Clear the back-off delay.
    send_backoff_.Reset();
  } else {
    // Increase the back-off delay.
    send_backoff_.InformOfRequest(false);
  }

  if (!send_buffers_.empty()) {
    DCHECK(!send_timer_.IsRunning());
    send_timer_.Start(FROM_HERE, send_backoff_.GetTimeUntilRelease(),
                      this, &TCPListenSocket::SendData);
  }
}

}  // namespace net
