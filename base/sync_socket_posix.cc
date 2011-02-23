// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include "base/file_util.h"
#include "base/logging.h"


namespace base {

namespace {
// To avoid users sending negative message lengths to Send/Receive
// we clamp message lengths, which are size_t, to no more than INT_MAX.
const size_t kMaxMessageLength = static_cast<size_t>(INT_MAX);

static const SyncSocket::Handle kInvalidHandle = -1;

}  // namespace

bool SyncSocket::CreatePair(SyncSocket* pair[2]) {
  Handle handles[2] = { kInvalidHandle, kInvalidHandle };
  SyncSocket* tmp_sockets[2] = { NULL, NULL };
#if defined(OS_MACOSX)
  int nosigpipe = 1;
#endif  // defined(OS_MACOSX)

  // Create the two SyncSocket objects first to avoid ugly cleanup issues.
  tmp_sockets[0] = new SyncSocket(kInvalidHandle);
  if (tmp_sockets[0] == NULL) {
    goto cleanup;
  }
  tmp_sockets[1] = new SyncSocket(kInvalidHandle);
  if (tmp_sockets[1] == NULL) {
    goto cleanup;
  }
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, handles) != 0) {
    goto cleanup;
  }
#if defined(OS_MACOSX)
  // On OSX an attempt to read or write to a closed socket may generate a
  // SIGPIPE rather than returning -1.  setsockopt will shut this off.
  if (0 != setsockopt(handles[0], SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe) ||
      0 != setsockopt(handles[1], SOL_SOCKET, SO_NOSIGPIPE,
                      &nosigpipe, sizeof nosigpipe)) {
    goto cleanup;
  }
#endif
  // Copy the handles out for successful return.
  tmp_sockets[0]->handle_ = handles[0];
  pair[0] = tmp_sockets[0];
  tmp_sockets[1]->handle_ = handles[1];
  pair[1] = tmp_sockets[1];
  return true;

 cleanup:
  if (handles[0] != kInvalidHandle) {
    if (HANDLE_EINTR(close(handles[0])) < 0)
      PLOG(ERROR) << "close";
  }
  if (handles[1] != kInvalidHandle) {
    if (HANDLE_EINTR(close(handles[1])) < 0)
      PLOG(ERROR) << "close";
  }
  delete tmp_sockets[0];
  delete tmp_sockets[1];
  return false;
}

bool SyncSocket::Close() {
  if (handle_ == kInvalidHandle) {
    return false;
  }
  int retval = HANDLE_EINTR(close(handle_));
  if (retval < 0)
    PLOG(ERROR) << "close";
  handle_ = kInvalidHandle;
  return (retval == 0);
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  DCHECK(length <= kMaxMessageLength);
  const char* charbuffer = static_cast<const char*>(buffer);
  int len = file_util::WriteFileDescriptor(handle_, charbuffer, length);
  return static_cast<size_t>(len);
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  DCHECK(length <= kMaxMessageLength);
  char* charbuffer = static_cast<char*>(buffer);
  if (file_util::ReadFromFD(handle_, charbuffer, length)) {
    return length;
  } else {
    return -1;
  }
}

size_t SyncSocket::Peek() {
  int number_chars;
  if (-1 == ioctl(handle_, FIONREAD, &number_chars)) {
    // If there is an error in ioctl, signal that the channel would block.
    return 0;
  }
  return (size_t) number_chars;
}

}  // namespace base
