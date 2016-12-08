/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "net/udp/udp_listen_socket.h"

#if defined(OS_POSIX)
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "net/base/net_errors.h"
#endif

#include "base/posix/eintr_wrapper.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/sys_byteorder.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "net/base/net_util.h"
#if defined(OS_STARBOARD)
#include "starboard/socket.h"
#endif

#if defined(__LB_SHELL__)
#include "lb_network_helpers.h"
#endif

namespace net {

#if defined(OS_POSIX)
const SocketDescriptor UDPListenSocket::kInvalidSocket = -1;
const int UDPListenSocket::kSocketError = -1;
#elif defined(OS_STARBOARD)
const SocketDescriptor UDPListenSocket::kInvalidSocket = kSbSocketInvalid;
const int UDPListenSocket::kSocketError = kSbSocketErrorFailed;
#endif

// 64kB is the max UDP packet size, but we don't need that
// much for SSDP headers. We keep this 1024 - 1.
const int kUdpMaxPacketSize = 1023;

UDPListenSocket::UDPListenSocket(SocketDescriptor s,
                                 UDPListenSocket::Delegate* del)
    : socket_delegate_(del),
      socket_(s),
      send_error_(false),
      buffer_(NULL) {
  WatchSocket();
}

UDPListenSocket::~UDPListenSocket() {
  CloseSocket(socket_);
}

void UDPListenSocket::Close() {
  UnwatchSocket();
  socket_delegate_->DidClose(this);
}

void UDPListenSocket::CloseSocket(SocketDescriptor s) {
  if (s != kInvalidSocket) {
    UnwatchSocket();
#if defined(OS_STARBOARD)
    SbSocketDestroy(s);
#else
    lb_close_socket(s);
#endif
  }
}

void UDPListenSocket::SendTo(const IPEndPoint& address,
                             const std::string& str) {
  SendTo(address, str.data(), static_cast<int>(str.length()));
}

void UDPListenSocket::SendTo(const IPEndPoint& address,
                             const char* bytes,
                             int len) {
#if defined(OS_STARBOARD)
  SbSocketAddress dst_addr;
  if (!address.ToSbSocketAddress(&dst_addr)) {
    DLOG(ERROR) << "Failed to convert IPEndPoint to SbSocketAddress. "
                << address.ToString();
    return;
  }

  len = std::min(len, kUdpMaxPacketSize);
  int sent = SbSocketSendTo(socket_, bytes, len, &dst_addr);
  if (sent != len) {
    DLOG(ERROR) << "SbSocketSendTo failed: errno== "
                << SbSocketGetLastError(socket_);
    send_error_ = true;
  }
#else   // defined(OS_STARBOARD)
  SockaddrStorage dst_addr;
  bool ret = address.ToSockAddr(dst_addr.addr, &dst_addr.addr_len);
  if (!ret) {
    LOG(ERROR) << "Failed to convert IPEndPoint to sockaddr. "
               << address.ToString();
    return;
  }

  len = std::min(len, kUdpMaxPacketSize);

  // TODO: check if flag can be MSG_DONTROUTE.
  int sent = HANDLE_EINTR(sendto(socket_, bytes, len, 0,
                                 dst_addr.addr, dst_addr.addr_len));
  if (sent != len) {
    LOG(ERROR) << "sendto failed: errno== " << errno;
    send_error_ = true;
  }
#endif  // defined(OS_STARBOARD)
}

void UDPListenSocket::Read() {
  if (buffer_ == NULL) {
    // +1 for null termination
    buffer_.reset(new char[kUdpMaxPacketSize + 1]);
  }

#if defined(OS_STARBOARD)
  do {
    SbSocketAddress src_addr;
    int len = SbSocketReceiveFrom(socket_, buffer_.get(), kUdpMaxPacketSize,
                                  &src_addr);
    if (len <= 0) {
      SbSocketError error = SbSocketGetLastError(socket_);
      if (error != kSbSocketPending) {
        DLOG(WARNING) << "SbSocketReceiveFrom failed: " << std::hex << error;
      }
      return;
    }

    DCHECK_LE(len, kUdpMaxPacketSize);
    IPEndPoint address;
    if (!address.FromSbSocketAddress(&src_addr)) {
      DLOG(ERROR) << "Failed to convert SbSocketAddress to IPEndPoint.";
      return;
    }

    buffer_[len] = '\0';  // Doesn't hurt to be careful!
    socket_delegate_->DidRead(this, buffer_.get(), len, &address);
  } while (true);
#else   // defined(OS_STARBOARD)
  int len;
  do {
    SockaddrStorage src_addr;
    len = HANDLE_EINTR(
        recvfrom(socket_, buffer_.get(),
                 kUdpMaxPacketSize, MSG_DONTWAIT,
                 src_addr.addr, &src_addr.addr_len));

    if (len <= 0) {
      if (!lb_net_would_block()) {
        PLOG(WARNING) << "recvfrom() failed: " << std::hex << len;
      }
      return;
    }

    if (len > 0) {
      DCHECK_LE(len, kUdpMaxPacketSize);

      IPEndPoint address;
      int ret = address.FromSockAddr(src_addr.addr, src_addr.addr_len);
      if (!ret) {
        LOG(ERROR) << "Failed to convert src sockaddr to IPEndPoint. ";
        return;
      }

      buffer_[len] = '\0'; // Doesn't hurt to be careful!
      socket_delegate_->DidRead(this, buffer_.get(), len, &address);
    }
  } while (true);
#endif  // defined(OS_STARBOARD)
}

void UDPListenSocket::WatchSocket() {
#if defined(OS_STARBOARD)
  MessageLoopForIO::current()->Watch(
      socket_, true, MessageLoopForIO::WATCH_READ, &watcher_, this);
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
  watcher_.StartWatching(socket_, base::MessagePumpShell::WATCH_READ, this);
#endif
}

void UDPListenSocket::UnwatchSocket() {
#if defined(OS_STARBOARD)
  watcher_.StopWatchingSocket();
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
  watcher_.StopWatching();
#endif
}

#if defined(OS_STARBOARD)
void UDPListenSocket::OnSocketReadyToRead(SbSocket /*socket*/) {
  Read();
}
#elif defined(__LB_SHELL__) && !defined(__LB_ANDROID__)
void UDPListenSocket::OnObjectSignaled(int object) {
  // Object watcher removes the object when it gets signaled, so start watching
  // again.
  watcher_.StartWatching(socket_, base::MessagePumpShell::WATCH_READ, this);

  Read();
}
#endif

} // namespace net
