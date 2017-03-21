// Copyright 2016 Google Inc. All Rights Reserved.
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

// The Starboard configuration for Desktop X86 Linux. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Linux.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_LINUX_X64DIRECTFB_CONFIGURATION_PUBLIC_H_
#define STARBOARD_LINUX_X64DIRECTFB_CONFIGURATION_PUBLIC_H_

// Reuse the configuration from Linux_x64, but we'll also enable the Blitter
// API afterwards since we'll have DirectFB available.
#include "starboard/linux/x64x11/configuration_public.h"

// Indicates whether or not the given platform supports rendering of NV12
// textures. These textures typically originate from video decoders.
#undef SB_HAS_NV12_TEXTURE_SUPPORT
#define SB_HAS_NV12_TEXTURE_SUPPORT 0

// This configuration supports the blitter API (implemented via DirectFB).
#undef SB_HAS_BLITTER
#define SB_HAS_BLITTER 1

// Unfortunately, DirectFB does not support bilinear filtering.  According to
// http://osdir.com/ml/graphics.directfb.user/2008-06/msg00028.html, "smooth
// scaling is not supported in conjunction with blending", and we need blending
// more.
#undef SB_HAS_BILINEAR_FILTERING_SUPPORT
#define SB_HAS_BILINEAR_FILTERING_SUPPORT 0

// DirectFB's only 32-bit RGBA color format is word-order ARGB.  This translates
// to byte-order ARGB for big endian platforms and byte-order BGRA for
// little-endian platforms.
#undef SB_PREFERRED_RGBA_BYTE_ORDER
#if SB_IS(BIG_ENDIAN)
#define SB_PREFERRED_RGBA_BYTE_ORDER SB_PREFERRED_RGBA_BYTE_ORDER_ARGB
#else
#define SB_PREFERRED_RGBA_BYTE_ORDER SB_PREFERRED_RGBA_BYTE_ORDER_BGRA
#endif

// The current platform has microphone supported.
#undef SB_HAS_MICROPHONE
#define SB_HAS_MICROPHONE 1

#endif  // STARBOARD_LINUX_X64DIRECTFB_CONFIGURATION_PUBLIC_H_
