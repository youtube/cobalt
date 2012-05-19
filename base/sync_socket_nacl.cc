// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>

#include "base/file_util.h"
#include "base/logging.h"


namespace base {

const SyncSocket::Handle SyncSocket::kInvalidHandle = -1;

SyncSocket::SyncSocket() : handle_(kInvalidHandle) {
  NOTREACHED();
}

SyncSocket::~SyncSocket() {
}

// static
bool SyncSocket::CreatePair(SyncSocket* socket_a, SyncSocket* socket_b) {
  return false;
}

bool SyncSocket::Close() {
 return false;
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  return 0;
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  return 0;
}

size_t SyncSocket::Peek() {
  return 0;
}

CancelableSyncSocket::CancelableSyncSocket() {}
CancelableSyncSocket::CancelableSyncSocket(Handle handle)
    : SyncSocket(handle) {
}

size_t CancelableSyncSocket::Send(const void* buffer, size_t length) {
  return 0;
}

bool CancelableSyncSocket::Shutdown() {
  return false;
}

// static
bool CancelableSyncSocket::CreatePair(CancelableSyncSocket* socket_a,
                                      CancelableSyncSocket* socket_b) {
  return SyncSocket::CreatePair(socket_a, socket_b);
}

}  // namespace base
