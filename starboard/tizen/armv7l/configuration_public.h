// Copyright 2016 Samsung Electronics. All Rights Reserved.
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

// The Starboard configuration for armv7l Tizen. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Tizen.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_TIZEN_ARMV7L_CONFIGURATION_PUBLIC_H_
#define STARBOARD_TIZEN_ARMV7L_CONFIGURATION_PUBLIC_H_

// The API version implemented by this platform.
#define SB_API_VERSION 4

// --- System Header Configuration -------------------------------------------

// Whether the current platform has microphone supported.
#define SB_HAS_MICROPHONE 0

// Whether the current platform has speech recognizer.
#define SB_HAS_SPEECH_RECOGNIZER 0

// Whether the current platform has speech synthesis.
#define SB_HAS_SPEECH_SYNTHESIS 0

// --- Architecture Configuration --------------------------------------------

// Whether the current platform is big endian. SB_IS_LITTLE_ENDIAN will be
// automatically set based on this.
#define SB_IS_BIG_ENDIAN 0

// Whether the current platform is an ARM architecture.
#define SB_IS_ARCH_ARM 1

// Whether the current platform is a MIPS architecture.
#define SB_IS_ARCH_MIPS 0

// Whether the current platform is a PPC architecture.
#define SB_IS_ARCH_PPC 0

// Whether the current platform is an x86 architecture.
#define SB_IS_ARCH_X86 0

// Whether the current platform is a 32-bit architecture.
#define SB_IS_32_BIT 1

#define SB_IS_64_BIT 0
// Whether the current platform's pointers are 32-bit.
// Whether the current platform's longs are 32-bit.
#if SB_IS(32_BIT)
#define SB_HAS_32_BIT_POINTERS 1
#define SB_HAS_32_BIT_LONG 1
#else
#define SB_HAS_32_BIT_POINTERS 0
#define SB_HAS_32_BIT_LONG 0
#endif

// Whether the current platform's pointers are 64-bit.
// Whether the current platform's longs are 64-bit.
#if SB_IS(64_BIT)
#define SB_HAS_64_BIT_POINTERS 1
#define SB_HAS_64_BIT_LONG 1
#else
#define SB_HAS_64_BIT_POINTERS 0
#define SB_HAS_64_BIT_LONG 0
#endif

// Configuration parameters that allow the application to make some general
// compile-time decisions with respect to the the number of cores likely to be
// available on this platform. For a definitive measure, the application should
// still call SbSystemGetNumberOfProcessors at runtime.

// Whether the current platform is expected to have many cores (> 6), or a
// wildly varying number of cores.
#define SB_HAS_MANY_CORES 1

// Whether the current platform is expected to have exactly 1 core.
#define SB_HAS_1_CORE 0

// Whether the current platform is expected to have exactly 2 cores.
#define SB_HAS_2_CORES 0

// Whether the current platform is expected to have exactly 4 cores.
#define SB_HAS_4_CORES 0

// Whether the current platform is expected to have exactly 6 cores.
#define SB_HAS_6_CORES 0

// Whether the current platform's thread scheduler will automatically balance
// threads between cores, as opposed to systems where threads will only ever run
// on the specifically pinned core.
#define SB_HAS_CROSS_CORE_SCHEDULER 1

// --- Media Configuration ---------------------------------------------------

// The maximum audio bitrate the platform can decode.  The following value
// equals to 2M bytes per seconds which is more than enough for compressed
// audio.
#define SB_MEDIA_MAX_AUDIO_BITRATE_IN_BITS_PER_SECOND (16 * 1024 * 1024)

// The maximum video bitrate the platform can decode.  The following value
// equals to 25M bytes per seconds which is more than enough for compressed
// video.
#define SB_MEDIA_MAX_VIDEO_BITRATE_IN_BITS_PER_SECOND (200 * 1024 * 1024)

// Specifies whether this platform has webm/vp9 support.  This should be set to
// non-zero on platforms with webm/vp9 support.
#define SB_HAS_MEDIA_WEBM_VP9_SUPPORT 0

// Specifies the stack size for threads created inside media stack.  Set to 0 to
// use the default thread stack size.  Set to non-zero to explicitly set the
// stack size for media stack threads.
#define SB_MEDIA_THREAD_STACK_SIZE 0U

// --- Decoder-only Params ---

// Specifies how media buffers must be aligned on this platform as some
// decoders may have special requirement on the alignment of buffers being
// decoded.
#define SB_MEDIA_BUFFER_ALIGNMENT 128U

// Specifies how video frame buffers must be aligned on this platform.
#define SB_MEDIA_VIDEO_FRAME_ALIGNMENT 256U

// The encoded video frames are compressed in different ways, their decoding
// time can vary a lot.  Occasionally a single frame can take longer time to
// decode than the average time per frame.  The player has to cache some frames
// to account for such inconsistency.  The number of frames being cached are
// controlled by the following two macros.
//
// Specify the number of video frames to be cached before the playback starts.
// Note that set this value too large may increase the playback start delay.
#define SB_MEDIA_MAXIMUM_VIDEO_PREROLL_FRAMES 4

// Specify the number of video frames to be cached during playback.  A large
// value leads to more stable fps but also causes the app to use more memory.
#define SB_MEDIA_MAXIMUM_VIDEO_FRAMES 12

// --- Common Configuration ---------------------------------------------------

// Include the Tizen configuration that's common between all Tizen.
#include "starboard/tizen/shared/configuration_public.h"

// --- User Configuration ----------------------------------------------------

// The maximum number of users that can be signed in at the same time.
#define SB_USER_MAX_SIGNED_IN 1

// --- Timing API ------------------------------------------------------------

// Whether this platform has an API to retrieve how long the current thread
// has spent in the executing state.
#define SB_HAS_TIME_THREAD_NOW 1

#endif  // STARBOARD_TIZEN_ARMV7L_CONFIGURATION_PUBLIC_H_
