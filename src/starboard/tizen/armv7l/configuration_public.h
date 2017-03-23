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

// Specifies whether this platform has support for a possibly-decrypting
// elementary stream player for at least H.264/AAC (and AES-128-CTR, if
// decrypting). A player is responsible for ingesting an audio and video
// elementary stream, optionally-encrypted, and ultimately producing
// synchronized audio/video. If a player is defined, it must choose one of the
// supported composition methods below.
#define SB_HAS_PLAYER 1

// Specifies whether this platform's player will produce an OpenGL texture that
// the client must draw every frame with its graphics rendering. It may be that
// we get a texture handle, but cannot perform operations like GlReadPixels on
// it if it is DRM-protected.
#define SB_IS_PLAYER_PRODUCING_TEXTURE 0

// Specifies whether this platform's player is composited with a formal
// compositor, where the client must specify how video is to be composited into
// the graphicals scene.
#define SB_IS_PLAYER_COMPOSITED 0

// Specifies whether this platform's player uses a "punch-out" model, where
// video is rendered to the far background, and the graphics plane is
// automatically composited on top of the video by the platform. The client must
// punch an alpha hole out of the graphics plane for video to show through.  In
// this case, changing the video bounds must be tightly synchronized between the
// player and the graphics plane.
#define SB_IS_PLAYER_PUNCHED_OUT 1

// Specifies the maximum amount of memory used by audio buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by audio buffers but will also make JavaScript app less likely to
// re-download audio data.  Note that the JavaScript app may experience
// significant difficulty if this value is too low.
#define SB_MEDIA_SOURCE_BUFFER_STREAM_AUDIO_MEMORY_LIMIT (3U * 1024U * 1024U)

// Specifies the maximum amount of memory used by video buffers of media source
// before triggering a garbage collection.  A large value will cause more memory
// being used by video buffers but will also make JavaScript app less likely to
// re-download video data.  Note that the JavaScript app may experience
// significant difficulty if this value is too low.
#define SB_MEDIA_SOURCE_BUFFER_STREAM_VIDEO_MEMORY_LIMIT (16U * 1024U * 1024U)

// Specifies how much memory to reserve up-front for the main media buffer
// (usually resides inside the CPU memory) used by media source and demuxers.
// The main media buffer can work in one of the following two ways:
// 1. If GPU buffer is used (i.e. SB_MEDIA_GPU_BUFFER_BUDGET is non-zero), the
//    main buffer will be used as a cache so a media buffer will be copied from
//    GPU memory to main memory before sending to the decoder for further
//    processing.  In this case this macro should be set to a value that is
//    large enough to hold all media buffers being decoded.
// 2. If GPU buffer is not used (i.e. SB_MEDIA_GPU_BUFFER_BUDGET is zero) all
//    media buffers will reside in the main memory buffer.  In this case the
//    macro should be set to a value that is greater than the sum of the above
//    source buffer stream memory limits with extra room to take account of
//    fragmentations and memory used by demuxers.
#define SB_MEDIA_MAIN_BUFFER_BUDGET (80U * 1024U * 1024U)

// Specifies how much GPU memory to reserve up-front for media source buffers.
// This should only be set to non-zero on system with limited CPU memory and
// excess GPU memory so the app can store media buffer in GPU memory.
// SB_MEDIA_MAIN_BUFFER_BUDGET has to be set to a non-zero value to avoid
// media buffers being decoded when being stored in GPU.
#define SB_MEDIA_GPU_BUFFER_BUDGET 0U

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

#endif  // STARBOARD_TIZEN_ARMV7L_CONFIGURATION_PUBLIC_H_
