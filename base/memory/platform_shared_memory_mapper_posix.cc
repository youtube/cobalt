// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_mapper.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

#include <sys/mman.h>

namespace base {

absl::optional<span<uint8_t>> PlatformSharedMemoryMapper::Map(
    subtle::PlatformSharedMemoryHandle handle,
    bool write_allowed,
    uint64_t offset,
    size_t size) {
  void* address =
      mmap(nullptr, size, PROT_READ | (write_allowed ? PROT_WRITE : 0),
           MAP_SHARED, handle.fd, checked_cast<off_t>(offset));

  if (address == MAP_FAILED) {
    DPLOG(ERROR) << "mmap " << handle.fd << " failed";
    return absl::nullopt;
  }

  return make_span(reinterpret_cast<uint8_t*>(address), size);
}

void PlatformSharedMemoryMapper::Unmap(span<uint8_t> mapping) {
  if (munmap(mapping.data(), mapping.size()) < 0)
    DPLOG(ERROR) << "munmap";
}

}  // namespace base
