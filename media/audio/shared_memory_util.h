// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_SHARED_MEMORY_UTIL_H_
#define MEDIA_AUDIO_SHARED_MEMORY_UTIL_H_

#include "base/basictypes.h"
#include "base/shared_memory.h"
#include "media/base/media_export.h"

namespace media {

// Value sent by the controller to the renderer in low-latency mode
// indicating that the stream is paused.
enum { kPauseMark = -1 };

// Functions that handle data buffer passed between processes in the shared
// memory.  Called on both IPC sides.  These are necessary because the shared
// memory has a layout: the last word in the block is the data size in bytes.

MEDIA_EXPORT uint32 TotalSharedMemorySizeInBytes(uint32 packet_size);
MEDIA_EXPORT uint32 PacketSizeInBytes(uint32 shared_memory_created_size);
MEDIA_EXPORT uint32 GetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                             uint32 packet_size);
MEDIA_EXPORT void SetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                           uint32 packet_size,
                                           uint32 actual_data_size);
MEDIA_EXPORT void SetActualDataSizeInBytes(void* shared_memory_ptr,
                                           uint32 packet_size,
                                           uint32 actual_data_size);
MEDIA_EXPORT void SetUnknownDataSize(base::SharedMemory* shared_memory,
                                     uint32 packet_size);
MEDIA_EXPORT bool IsUnknownDataSize(base::SharedMemory* shared_memory,
                                    uint32 packet_size);

}  // namespace media

#endif  // MEDIA_AUDIO_SHARED_MEMORY_UTIL_H_
