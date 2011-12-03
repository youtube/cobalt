// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_UTIL_H_
#define MEDIA_AUDIO_AUDIO_UTIL_H_

#include <vector>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace base {
class SharedMemory;
}

namespace media {

// For all audio functions 3 audio formats are supported:
// 8 bits unsigned 0 to 255.
// 16 bit signed (little endian).
// 32 bit signed (little endian)

// AdjustVolume() does a software volume adjustment of a sample buffer.
// The samples are multiplied by the volume, which should range from
// 0.0 (mute) to 1.0 (full volume).
// Using software allows each audio and video to have its own volume without
// affecting the master volume.
// In the future the function may be used to adjust the sample format to
// simplify hardware requirements and to support a wider variety of input
// formats.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
MEDIA_EXPORT bool AdjustVolume(void* buf,
                               size_t buflen,
                               int channels,
                               int bytes_per_sample,
                               float volume);

// FoldChannels() does a software multichannel folding down to stereo.
// Channel order is assumed to be 5.1 Dolby standard which is
// front left, front right, center, surround left, surround right.
// The subwoofer is ignored.
// 6.1 adds a rear center speaker, and 7.1 has 2 rear speakers.  These
// channels are rare and ignored.
// After summing the channels, volume is adjusted and the samples are
// clipped to the maximum value.
// Volume should normally range from 0.0 (mute) to 1.0 (full volume), but
// since clamping is performed a value of more than 1 is allowed to increase
// volume.
// The buffer is modified in-place to avoid memory management, as this
// function may be called in performance critical code.
MEDIA_EXPORT bool FoldChannels(void* buf,
                               size_t buflen,
                               int channels,
                               int bytes_per_sample,
                               float volume);

// DeinterleaveAudioChannel() takes interleaved audio buffer |source|
// of the given |sample_fmt| and |number_of_channels| and extracts
// |number_of_frames| data for the given |channel_index| and
// puts it in the floating point |destination|.
// It returns |true| on success, or |false| if the |sample_fmt| is
// not recognized.
MEDIA_EXPORT bool DeinterleaveAudioChannel(void* source,
                                           float* destination,
                                           int channels,
                                           int channel_index,
                                           int bytes_per_sample,
                                           size_t number_of_frames);

// InterleaveFloatToInt16 scales, clips, and interleaves the planar
// floating-point audio contained in |source| to the int16 |destination|.
// The floating-point data is in a canonical range of -1.0 -> +1.0.
// The size of the |source| vector determines the number of channels.
// The |destination| buffer is assumed to be large enough to hold the
// result. Thus it must be at least size: number_of_frames * source.size()
MEDIA_EXPORT void InterleaveFloatToInt16(const std::vector<float*>& source,
                                         int16* destination,
                                         size_t number_of_frames);

// Returns the default audio output hardware sample-rate.
MEDIA_EXPORT double GetAudioHardwareSampleRate();

// Returns the default audio input hardware sample-rate.
MEDIA_EXPORT double GetAudioInputHardwareSampleRate();

// Returns the optimal low-latency buffer size for the audio hardware.
// This is the smallest buffer size the system can comfortably render
// at without glitches.  The buffer size is in sample-frames.
MEDIA_EXPORT size_t GetAudioHardwareBufferSize();

// Functions that handle data buffer passed between processes in the shared
// memory. Called on both IPC sides.

MEDIA_EXPORT uint32 TotalSharedMemorySizeInBytes(uint32 packet_size);
MEDIA_EXPORT uint32 PacketSizeSizeInBytes(uint32 shared_memory_created_size);
MEDIA_EXPORT uint32 GetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                             uint32 shared_memory_size);
MEDIA_EXPORT void SetActualDataSizeInBytes(base::SharedMemory* shared_memory,
                                           uint32 shared_memory_size,
                                           uint32 actual_data_size);
MEDIA_EXPORT void SetUnknownDataSize(base::SharedMemory* shared_memory,
                                     uint32 shared_memory_size);
MEDIA_EXPORT bool IsUnknownDataSize(base::SharedMemory* shared_memory,
                                    uint32 shared_memory_size);

#if defined(OS_WIN)

// Does Windows support WASAPI? We are checking in lot of places, and
// sometimes check was written incorrectly, so move into separate function.
MEDIA_EXPORT bool IsWASAPISupported();

#endif  // defined(OS_WIN)

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_UTIL_H_
