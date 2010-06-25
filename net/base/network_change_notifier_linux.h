// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
#define NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "net/base/network_change_notifier.h"

namespace base {
class Thread;
}

namespace net {

class NetworkChangeNotifierLinux : public MessageLoop::DestructionObserver,
                                   public MessageLoopForIO::Watcher,
                                   public NetworkChangeNotifier {
 public:
  NetworkChangeNotifierLinux();

 private:
  virtual ~NetworkChangeNotifierLinux();

  // MessageLoop::DestructionObserver:
  virtual void WillDestroyCurrentMessageLoop();

  // MessageLoopForIO::Watcher:
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int /* fd */);

  // Called on the notifier thread to initialize the notification
  // implementation.
  void Init();

  // Starts listening for netlink messages.  Also handles the messages if there
  // are any available on the netlink socket.
  void ListenForNotifications();

  // Attempts to read from the netlink socket into |buf| of length |len|.
  // Returns the bytes read on synchronous success and ERR_IO_PENDING if the
  // recv() would block.  Otherwise, it returns a net error code.
  int ReadNotificationMessage(char* buf, size_t len);

  // The thread used to listen for notifications.  This relays the notification
  // to the registered observers without posting back to the thread the object
  // was created on.
  scoped_ptr<base::Thread> notifier_thread_;

  // The netlink socket descriptor.
  int netlink_fd_;

  MessageLoopForIO::FileDescriptorWatcher netlink_watcher_;

  DISALLOW_COPY_AND_ASSIGN(NetworkChangeNotifierLinux);
};

}  // namespace net

#endif  // NET_BASE_NETWORK_CHANGE_NOTIFIER_LINUX_H_
