// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <errno.h>
#include <sys/socket.h>

#include "base/eintr_wrapper.h"
#include "base/task.h"
#include "base/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier_netlink_linux.h"

// We only post tasks to a child thread we own, so we don't need refcounting.
DISABLE_RUNNABLE_METHOD_REFCOUNT(net::NetworkChangeNotifierLinux);

namespace net {

namespace {

const int kInvalidSocket = -1;

}  // namespace

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux()
    : notifier_thread_(new base::Thread("NetworkChangeNotifier")),
      netlink_fd_(kInvalidSocket) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
  notifier_thread_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &NetworkChangeNotifierLinux::Init));
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // We don't need to explicitly Stop(), but doing so allows us to sanity-
  // check that the notifier thread shut down properly.
  notifier_thread_->Stop();
  DCHECK_EQ(kInvalidSocket, netlink_fd_);
}

void NetworkChangeNotifierLinux::WillDestroyCurrentMessageLoop() {
  DCHECK(notifier_thread_ != NULL);
  // We can't check the notifier_thread_'s message_loop(), as it's now 0.
  // DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  if (netlink_fd_ != kInvalidSocket) {
    if (HANDLE_EINTR(close(netlink_fd_)) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    netlink_watcher_.StopWatchingFileDescriptor();
  }
}

void NetworkChangeNotifierLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  DCHECK_EQ(fd, netlink_fd_);
  ListenForNotifications();
}

void NetworkChangeNotifierLinux::OnFileCanWriteWithoutBlocking(int /* fd */) {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  NOTREACHED();
}

void NetworkChangeNotifierLinux::Init() {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  netlink_fd_ = InitializeNetlinkSocket();
  if (netlink_fd_ < 0) {
    netlink_fd_ = kInvalidSocket;
    return;
  }
  MessageLoop::current()->AddDestructionObserver(this);
  ListenForNotifications();
}

void NetworkChangeNotifierLinux::ListenForNotifications() {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  char buf[4096];
  int rv = ReadNotificationMessage(buf, arraysize(buf));
  while (rv > 0) {
    if (HandleNetlinkMessage(buf, rv)) {
      LOG(INFO) << "Detected IP address changes.";
#if defined(OS_CHROMEOS)
      // TODO(zelidrag): chromium-os:3996 - introduced artificial delay to
      // work around the issue of proxy initialization before name resolving
      // is functional in ChromeOS. This should be removed once this bug
      // is properly fixed.
      const int kObserverNotificationDelayMS = 500;
      MessageLoop::current()->PostDelayedTask(FROM_HERE, NewRunnableFunction(
          &NetworkChangeNotifier::NotifyObserversOfIPAddressChange),
          kObserverNotificationDelayMS);
#else
      NotifyObserversOfIPAddressChange();
#endif
    }
    rv = ReadNotificationMessage(buf, arraysize(buf));
  }

  if (rv == ERR_IO_PENDING) {
    rv = MessageLoopForIO::current()->WatchFileDescriptor(netlink_fd_, false,
        MessageLoopForIO::WATCH_READ, &netlink_watcher_, this);
    LOG_IF(ERROR, !rv) << "Failed to watch netlink socket: " << netlink_fd_;
  }
}

int NetworkChangeNotifierLinux::ReadNotificationMessage(char* buf, size_t len) {
  DCHECK(notifier_thread_ != NULL);
  DCHECK_EQ(notifier_thread_->message_loop(), MessageLoop::current());

  DCHECK_NE(len, 0u);
  DCHECK(buf);
  memset(buf, 0, sizeof(buf));
  int rv = recv(netlink_fd_, buf, len, 0);
  if (rv > 0)
    return rv;

  DCHECK_NE(rv, 0);
  if (errno != EAGAIN && errno != EWOULDBLOCK) {
    PLOG(DFATAL) << "recv";
    return ERR_FAILED;
  }

  return ERR_IO_PENDING;
}

}  // namespace net
