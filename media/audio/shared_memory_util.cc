// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/shared_memory_util.h"

#include "base/atomicops.h"
#include "base/logging.h"

using base::subtle::Atomic32;

static const uint32 kUnknownDataSize = static_cast<uint32>(-1);

namespace media {

uint32 TotalSharedMemorySizeInBytes(uint32 packet_size) {
  // Need to reserve extra 4 bytes for size of data.
  return packet_size + sizeof(Atomic32);
}

uint32 PacketSizeInBytes(uint32 shared_memory_created_size) {
  return shared_memory_created_size - sizeof(Atomic32);
}

uint32 GetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                uint32 packet_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + packet_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Actual data size stored at the end of the buffer.
  uint32 actual_data_size =
      base::subtle::Acquire_Load(reinterpret_cast<volatile Atomic32*>(ptr));
  return std::min(actual_data_size, packet_size);
}

void SetActualDataSizeInBytes(void* shared_memory_ptr,
                              uint32 packet_size,
                              uint32 actual_data_size) {
  char* ptr = static_cast<char*>(shared_memory_ptr) + packet_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Set actual data size at the end of the buffer.
  base::subtle::Release_Store(reinterpret_cast<volatile Atomic32*>(ptr),
                              actual_data_size);
}

void SetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                              uint32 packet_size,
                              uint32 actual_data_size) {
  SetActualDataSizeInBytes(shared_memory->memory(),
                           packet_size, actual_data_size);
}

void SetUnknownDataSize(base::SharedMemory* shared_memory,
                        uint32 packet_size) {
  SetActualDataSizeInBytes(shared_memory, packet_size, kUnknownDataSize);
}

bool IsUnknownDataSize(base::SharedMemory* shared_memory,
                       uint32 packet_size) {
  char* ptr = static_cast<char*>(shared_memory->memory()) + packet_size;
  DCHECK_EQ(0u, reinterpret_cast<size_t>(ptr) & 3);

  // Actual data size stored at the end of the buffer.
  uint32 actual_data_size =
      base::subtle::Acquire_Load(reinterpret_cast<volatile Atomic32*>(ptr));
  return actual_data_size == kUnknownDataSize;
}

}  // namespace media
