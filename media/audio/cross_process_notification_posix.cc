// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/cross_process_notification.h"

#include <errno.h>
#include <sys/poll.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/file_descriptor_posix.h"

CrossProcessNotification::~CrossProcessNotification() {}

CrossProcessNotification::CrossProcessNotification(IPCHandle handle_1,
                                                   IPCHandle handle_2)
    : socket_(handle_1.fd) {
  DCHECK_NE(handle_1.fd, -1);
  DCHECK_EQ(handle_2.fd, -1);
  DCHECK(IsValid());
}

void CrossProcessNotification::Signal() {
  DCHECK(IsValid());
  char signal = 1;
  size_t bytes = socket_.Send(&signal, sizeof(signal));
  DCHECK_EQ(bytes, 1U) << "errno: " << errno;
}

void CrossProcessNotification::Wait() {
  DCHECK(IsValid());
  char signal = 0;
  size_t bytes = socket_.Receive(&signal, sizeof(signal));
  DCHECK_EQ(bytes, 1U) << "errno: " << errno;
  DCHECK_EQ(signal, 1);
}

bool CrossProcessNotification::IsValid() const {
  return socket_.handle() != SocketClass::kInvalidHandle;
}

bool CrossProcessNotification::ShareToProcess(base::ProcessHandle process,
                                              IPCHandle* handle_1,
                                              IPCHandle* handle_2) {
  DCHECK(IsValid());
  handle_1->fd = socket_.handle();
  handle_1->auto_close = false;
  handle_2->fd = -1;
  return true;
}

// static
bool CrossProcessNotification::InitializePair(CrossProcessNotification* a,
                                              CrossProcessNotification* b) {
  DCHECK(!a->IsValid());
  DCHECK(!b->IsValid());

  bool ok = SocketClass::CreatePair(&a->socket_, &b->socket_);

  DLOG_IF(WARNING, !ok) << "failed to create socket: " << errno;
  DCHECK(!ok || a->IsValid());
  DCHECK(!ok || b->IsValid());
  return ok;
}

// static
int CrossProcessNotification::WaitMultiple(const Notifications& notifications,
                                           size_t wait_offset) {
  DCHECK_LT(wait_offset, notifications.size());

  for (size_t i = 0; i < notifications.size(); ++i) {
    DCHECK(notifications[i]->IsValid());
  }

  // Below, we always check the |revents| of the first socket in the array
  // and return the index of that socket if set.  This can cause sockets
  // that come later in the array to starve when the first sockets are
  // very busy.  So to avoid the starving problem, we use the |wait_offset|
  // variable to split up the array so that the last socket to be signaled
  // becomes the last socket in the array and all the other sockets will have
  // priority the next time WaitMultiple is called.
  scoped_array<struct pollfd> sockets(new struct pollfd[notifications.size()]);
  memset(&sockets[0], 0, notifications.size() * sizeof(sockets[0]));
  size_t index = 0;
  for (size_t i = wait_offset; i < notifications.size(); ++i) {
    struct pollfd& fd = sockets[index++];
    fd.events = POLLIN;
    fd.fd = notifications[i]->socket_.handle();
  }

  for (size_t i = 0; i < wait_offset; ++i) {
    struct pollfd& fd = sockets[index++];
    fd.events = POLLIN;
    fd.fd = notifications[i]->socket_.handle();
  }
  DCHECK_EQ(index, notifications.size());

  int err = poll(&sockets[0], notifications.size(), -1);
  if (err != -1) {
    for (size_t i = 0; i < notifications.size(); ++i) {
      if (sockets[i].revents) {
        size_t ret = (i + wait_offset) % notifications.size();
        DCHECK_EQ(sockets[i].fd, notifications[ret]->socket_.handle());
        notifications[ret]->Wait();
        return ret;
      }
    }
  }
  // Either poll() failed or we failed to find a single socket that was
  // signaled.  Either way continuing will result in undefined behavior.
  LOG(FATAL) << "poll() failed: " << errno;
  return -1;
}
