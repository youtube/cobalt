// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_linux.h"

#include <errno.h>
#include <sys/socket.h>

#include "base/compiler_specific.h"
#include "base/eintr_wrapper.h"
#include "base/task.h"
#include "base/threading/thread.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier_netlink_linux.h"

namespace net {

namespace {

const int kInvalidSocket = -1;

}  // namespace

class NetworkChangeNotifierLinux::Thread
    : public base::Thread, public MessageLoopForIO::Watcher {
 public:
  Thread();
  virtual ~Thread();

  // MessageLoopForIO::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int /* fd */);

 protected:
  // base::Thread
  virtual void Init();
  virtual void CleanUp();

 private:
  void NotifyObserversOfIPAddressChange() {
    NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
  }

  // Starts listening for netlink messages.  Also handles the messages if there
  // are any available on the netlink socket.
  void ListenForNotifications();

  // Attempts to read from the netlink socket into |buf| of length |len|.
  // Returns the bytes read on synchronous success and ERR_IO_PENDING if the
  // recv() would block.  Otherwise, it returns a net error code.
  int ReadNotificationMessage(char* buf, size_t len);

  // The netlink socket descriptor.
  int netlink_fd_;
  MessageLoopForIO::FileDescriptorWatcher netlink_watcher_;

  // Technically only needed for ChromeOS, but it's ugly to #ifdef out.
  ScopedRunnableMethodFactory<Thread> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(Thread);
};

NetworkChangeNotifierLinux::Thread::Thread()
    : base::Thread("NetworkChangeNotifier"),
      netlink_fd_(kInvalidSocket),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {}

NetworkChangeNotifierLinux::Thread::~Thread() {}

void NetworkChangeNotifierLinux::Thread::Init() {
  netlink_fd_ = InitializeNetlinkSocket();
  if (netlink_fd_ < 0) {
    netlink_fd_ = kInvalidSocket;
    return;
  }
  ListenForNotifications();
}

void NetworkChangeNotifierLinux::Thread::CleanUp() {
  if (netlink_fd_ != kInvalidSocket) {
    if (HANDLE_EINTR(close(netlink_fd_)) != 0)
      PLOG(ERROR) << "Failed to close socket";
    netlink_fd_ = kInvalidSocket;
    netlink_watcher_.StopWatchingFileDescriptor();
  }
}

void NetworkChangeNotifierLinux::Thread::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK_EQ(fd, netlink_fd_);
  ListenForNotifications();
}

void NetworkChangeNotifierLinux::Thread::OnFileCanWriteWithoutBlocking(
    int /* fd */) {
  NOTREACHED();
}

void NetworkChangeNotifierLinux::Thread::ListenForNotifications() {
  char buf[4096];
  int rv = ReadNotificationMessage(buf, arraysize(buf));
  while (rv > 0) {
    if (HandleNetlinkMessage(buf, rv)) {
      VLOG(1) << "Detected IP address changes.";
#if defined(OS_CHROMEOS)
      // TODO(oshima): chromium-os:8285 - introduced artificial delay to
      // work around the issue of network load issue after connection
      // restored. See the bug for more details.
      //  This should be removed once this bug is properly fixed.
      const int kObserverNotificationDelayMS = 200;
      message_loop()->PostDelayedTask(
          FROM_HERE,
          method_factory_.NewRunnableMethod(
              &Thread::NotifyObserversOfIPAddressChange),
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

int NetworkChangeNotifierLinux::Thread::ReadNotificationMessage(
    char* buf,
    size_t len) {
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

NetworkChangeNotifierLinux::NetworkChangeNotifierLinux()
    : notifier_thread_(new Thread) {
  // We create this notifier thread because the notification implementation
  // needs a MessageLoopForIO, and there's no guarantee that
  // MessageLoop::current() meets that criterion.
  base::Thread::Options thread_options(MessageLoop::TYPE_IO, 0);
  notifier_thread_->StartWithOptions(thread_options);
}

NetworkChangeNotifierLinux::~NetworkChangeNotifierLinux() {
  // We don't need to explicitly Stop(), but doing so allows us to sanity-
  // check that the notifier thread shut down properly.
  notifier_thread_->Stop();
}

bool NetworkChangeNotifierLinux::IsCurrentlyOffline() const {
  // TODO(eroman): http://crbug.com/53473
  return false;
}

}  // namespace net
