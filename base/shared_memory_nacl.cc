// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/shared_memory.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"

namespace base {

SharedMemory::SharedMemory()
    : mapped_file_(-1),
      mapped_size_(0),
      inode_(0),
      memory_(NULL),
      read_only_(false),
      created_size_(0) {
  NOTREACHED();
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only)
    : mapped_file_(handle.fd),
      mapped_size_(0),
      inode_(0),
      memory_(NULL),
      read_only_(read_only),
      created_size_(0) {
  NOTREACHED();
}

SharedMemory::SharedMemory(SharedMemoryHandle handle, bool read_only,
                           ProcessHandle process)
    : mapped_file_(handle.fd),
      mapped_size_(0),
      inode_(0),
      memory_(NULL),
      read_only_(read_only),
      created_size_(0) {
  NOTREACHED();
}

SharedMemory::~SharedMemory() {
}

// static
bool SharedMemory::IsHandleValid(const SharedMemoryHandle& handle) {
  NOTREACHED();
  return false;
}

// static
SharedMemoryHandle SharedMemory::NULLHandle() {
  return SharedMemoryHandle();
}

// static
void SharedMemory::CloseHandle(const SharedMemoryHandle& handle) {
}

bool SharedMemory::CreateAndMapAnonymous(uint32 size) {
  return false;
}

bool SharedMemory::Create(const SharedMemoryCreateOptions& options) {
  return false;
}

// Our current implementation of shmem is with mmap()ing of files.
// These files need to be deleted explicitly.
// In practice this call is only needed for unit tests.
bool SharedMemory::Delete(const std::string& name) {
  return false;
}

bool SharedMemory::Open(const std::string& name, bool read_only) {
  return false;
}

bool SharedMemory::Map(uint32 bytes) {
  return false;
}

bool SharedMemory::Unmap() {
  return false;
}

SharedMemoryHandle SharedMemory::handle() const {
  return FileDescriptor(mapped_file_, false);
}

void SharedMemory::Close() {
}

void SharedMemory::Lock() {
}

void SharedMemory::Unlock() {
}

bool SharedMemory::PrepareMapFile(FILE *fp) {
  return false;
}

bool SharedMemory::FilePathForMemoryName(const std::string& mem_name,
                                         FilePath* path) {
  return false;
}

void SharedMemory::LockOrUnlockCommon(int function) {
}

bool SharedMemory::ShareToProcessCommon(ProcessHandle process,
                                        SharedMemoryHandle *new_handle,
                                        bool close_self) {
  return false;
}

}  // namespace base
