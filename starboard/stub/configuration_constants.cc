// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This file defines all configuration constants for a platform.

#include "starboard/configuration_constants.h"

// Determines the threshhold of allocation size that should be done with mmap
// (if available), rather than allocated within the core heap.
const size_t kSbDefaultMmapThreshold = 256 * 1024U;

// The current platform's maximum length of the name of a single directory
// entry, not including the absolute path.
const int32_t kSbFileMaxName = 64;

// Determines the alignment that allocations should have on this platform.
const size_t kSbMallocAlignment = 16;

// The maximum length of a name for a thread, including the NULL-terminator.
const int32_t kSbMaxThreadNameLength = 16;

// Defines the path where memory debugging logs should be written to.
const char* kSbMemoryLogPath = "/tmp/starboard";

// The maximum audio bitrate the platform can decode.  The following value
// equals to 5M bytes per seconds which is more than enough for compressed
// audio.
const uint32_t kSbMediaMaxAudioBitrateInBitsPerSecond = 40 * 1024 * 1024;

// The maximum video bitrate the platform can decode.  The following value
// equals to 25M bytes per seconds which is more than enough for compressed
// video.
const uint32_t kSbMediaMaxVideoBitrateInBitsPerSecond = 200 * 1024 * 1024;

// Specify the number of video frames to be cached during playback.  A large
// value leads to more stable fps but also causes the app to use more memory.
const uint32_t kSbMediaMaximumVideoFrames = 12;

// The encoded video frames are compressed in different ways, so their decoding
// time can vary a lot.  Occasionally a single frame can take longer time to
// decode than the average time per frame.  The player has to cache some frames
// to account for such inconsistency.  The number of frames being cached are
// controlled by SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES and
// SB_MEDIA_MAXIMUM_VIDEO_FRAMES.
//
// Specify the number of video frames to be cached before the playback starts.
// Note that setting this value too large may increase the playback start delay.
const uint32_t kSbMediaMaximumVideoPrerollFrames = 4;

// Specifies how video frame buffers must be aligned on this platform.
const uint32_t kSbMediaVideoFrameAlignment = 256;

// The memory page size, which controls the size of chunks on memory that
// allocators deal with, and the alignment of those chunks. This doesn't have to
// be the hardware-defined physical page size, but it should be a multiple of
// it.
const size_t kSbMemoryPageSize = 4096;

// Specifies the network receive buffer size in bytes, set via
// SbSocketSetReceiveBufferSize().
//
// Setting this to 0 indicates that SbSocketSetReceiveBufferSize() should
// not be called. Use this for OSs (such as Linux) where receive buffer
// auto-tuning is better.
//
// On some platforms, this may affect max TCP window size which may
// dramatically affect throughput in the presence of latency.
//
// If your platform does not have a good TCP auto-tuning mechanism,
// a setting of (128 * 1024) here is recommended.
const uint32_t kSbNetworkReceiveBufferSize = 0;

// The maximum number of users that can be signed in at the same time.
const uint32_t kSbUserMaxSignedIn = 1;
