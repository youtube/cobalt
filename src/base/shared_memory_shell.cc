// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include "base/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/threading/platform_thread.h"
#include "base/safe_strerror_posix.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/utf_string_conversions.h"

namespace base {

SharedMemory::SharedMemory()
    : memory_(NULL),
      read_only_(false),
      created_size_(0) {
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : handle_(handle),
      memory_(NULL),
      read_only_(read_only),
      created_size_(0) {
}

SharedMemory::~SharedMemory() {
  Close();
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  return handle.get() != NULL;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
  // Nothing to worry about here since shared memory are refcounted
}

bool SharedMemory::CreateAndMapAnonymous(size_t size) {
  return CreateAnonymous(size) && Map(size);
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  handle_ = new RefCountedMem(options.size);

  return true;
}

bool SharedMemory::Delete(const std::string& name) {
  NOTREACHED();
  return false;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  NOTREACHED();
  return false;
}

bool SharedMemory::Map(size_t bytes) {
  DCHECK(bytes <= handle_->GetSize());
  memory_ = handle_->GetMemory();
  return true;
}

bool SharedMemory::Unmap() {
  memory_ = NULL;
  return true;
}

SharedMemoryHandle SharedMemory::handle() const {
  return handle_.get();
}

void SharedMemory::Close() {
  handle_ = NULL;
}

void SharedMemory::Lock() {
  handle_->lock().Acquire();
}

void SharedMemory::Unlock() {
  handle_->lock().Release();
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle *new_handle,
                                        bool close_self) {
  // close_self doesn't matter because the shared memory instances are
  // ref counted
  *new_handle = handle_;

  return true;
}

}  // namespace base
