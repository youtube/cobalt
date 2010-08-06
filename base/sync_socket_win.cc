// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sync_socket.h"
#include <limits.h>
#include <stdio.h>
#include <windows.h>
#include <sys/types.h>
#include "base/logging.h"


namespace base {

namespace {
// This prefix used to be appended to pipe names for pipes
// created in CreatePair.
const wchar_t kPipePrefix[] = L"\\\\.\\pipe\\chrome.sync.";
const size_t kPipePrefixSize = arraysize(kPipePrefix);
const size_t kPathMax = 28;  // print length of process id + pair count.
const size_t kPipePathMax = kPipePrefixSize + kPathMax + 1;

// To avoid users sending negative message lengths to Send/Receive
// we clamp message lengths, which are size_t, to no more than INT_MAX.
const size_t kMaxMessageLength = static_cast<size_t>(INT_MAX);

const int kOutBufferSize = 4096;
const int kInBufferSize = 4096;
const int kDefaultTimeoutMilliSeconds = 1000;

static const SyncSocket::Handle kInvalidHandle = INVALID_HANDLE_VALUE;

}  // namespace

bool SyncSocket::CreatePair(SyncSocket* pair[2]) {
  Handle handles[2];
  SyncSocket* tmp_sockets[2];

  // Create the two SyncSocket objects first to avoid ugly cleanup issues.
  tmp_sockets[0] = new SyncSocket(kInvalidHandle);
  if (tmp_sockets[0] == NULL) {
    return false;
  }
  tmp_sockets[1] = new SyncSocket(kInvalidHandle);
  if (tmp_sockets[1] == NULL) {
    delete tmp_sockets[0];
    return false;
  }

  wchar_t name[kPipePathMax];
  do {
    unsigned int rnd_name;
    if (rand_s(&rnd_name) != 0)
      return false;
    swprintf(name, kPipePathMax, L"%s%u.%lu",
             kPipePrefix, GetCurrentProcessId(),
             rnd_name);
    handles[0] = CreateNamedPipeW(
        name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
        1,
        kOutBufferSize,
        kInBufferSize,
        kDefaultTimeoutMilliSeconds,
        NULL);
    if (handles[0] == INVALID_HANDLE_VALUE &&
        GetLastError() != ERROR_ACCESS_DENIED &&
        GetLastError() != ERROR_PIPE_BUSY) {
      return false;
    }
  } while (handles[0] == INVALID_HANDLE_VALUE);
  handles[1] = CreateFileW(name,
                           GENERIC_READ | GENERIC_WRITE,
                           0,              // no sharing.
                           NULL,           // default security attributes.
                           OPEN_EXISTING,  // opens existing pipe.
                           SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
                                           // no impersonation.
                           NULL);          // no template file.
  if (handles[1] == INVALID_HANDLE_VALUE) {
    CloseHandle(handles[0]);
    return false;
  }
  if (ConnectNamedPipe(handles[0], NULL) == FALSE) {
    DWORD error = GetLastError();
    if (error != ERROR_PIPE_CONNECTED) {
      CloseHandle(handles[0]);
      CloseHandle(handles[1]);
      return false;
    }
  }
  // Copy the handles out for successful return.
  tmp_sockets[0]->handle_ = handles[0];
  pair[0] = tmp_sockets[0];
  tmp_sockets[1]->handle_ = handles[1];
  pair[1] = tmp_sockets[1];
  return true;
}

bool SyncSocket::Close() {
  if (handle_ == kInvalidHandle) {
    return false;
  }
  BOOL retval = CloseHandle(handle_);
  handle_ = kInvalidHandle;
  return retval ? true : false;
}

size_t SyncSocket::Send(const void* buffer, size_t length) {
  DCHECK(length <= kMaxMessageLength);
  size_t count = 0;
  while (count < length) {
    DWORD len;
    // The following statement is for 64 bit portability.
    DWORD chunk = static_cast<DWORD>(
      ((length - count) <= UINT_MAX) ? (length - count) : UINT_MAX);
    if (WriteFile(handle_, static_cast<const char*>(buffer) + count,
                  chunk, &len, NULL) == FALSE) {
      return (0 < count) ? count : 0;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::Receive(void* buffer, size_t length) {
  DCHECK(length <= kMaxMessageLength);
  size_t count = 0;
  while (count < length) {
    DWORD len;
    DWORD chunk = static_cast<DWORD>(
      ((length - count) <= UINT_MAX) ? (length - count) : UINT_MAX);
    if (ReadFile(handle_, static_cast<char*>(buffer) + count,
                 chunk, &len, NULL) == FALSE) {
      return (0 < count) ? count : 0;
    }
    count += len;
  }
  return count;
}

size_t SyncSocket::Peek() {
  DWORD available = 0;
  PeekNamedPipe(handle_, NULL, 0, NULL, &available, NULL);
  return available;
}

}  // namespace base
