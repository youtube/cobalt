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

// Module Overview: Starboard Image Decoding Module
//
// API for hardware accelerated image decoding. This module allows for the
// client to feed in raw, encoded data to be decoded directly into an
// SbDecodeTarget.  It also provides an interface for the client to query what
// combinations of encoded image formats and SbDecodeTargetFormats are
// supported or not.
//
// All functions in this module are safe to call from any thread at any point
// in time.
//
// # SbImageIsDecodeSupported and SbImageDecode Example
//
// Let's assume that we're on a Blitter platform.
//
//     SbDecodeTargetProvider* provider = GetProviderFromSomewhere();
//     void* data = GetCompressedJPEGFromSomewhere();
//     int data_size = GetCompressedJPEGSizeFromSomewhere();
//     const char* mime_type = "image/jpeg";
//     SbDecodeTargetFormat format = kSbDecodeTargetFormat1PlaneRGBA;
//
//     if (!SbImageIsDecodeSupported(mime_type, format)) {
//       return;
//     }
//
//     SbDecodeTarget result_target = SbDecodeImage(provider, data, data_size,
//                                                  mime_type, format);
//     SbBlitterSurface surface =
//         SbDecodeTargetGetPlane(target, kSbDecodeTargetPlaneRGBA);
//     // Do stuff with surface...
//

#ifndef STARBOARD_IMAGE_H_
#define STARBOARD_IMAGE_H_

#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/export.h"
#include "starboard/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// Whether the current platform supports hardware accelerated decoding an
// image of mime type |mime_type| into SbDecodeTargetFormat |format|.  The
// result of this function must not change over the course of the program,
// which means that the results of this function may be cached indefinitely.
SB_EXPORT bool SbImageIsDecodeSupported(const char* mime_type,
                                        SbDecodeTargetFormat format);

// Attempt to decode encoded |mime_type| image data |data| of size |data_size|
// into an SbDecodeTarget of SbDecodeFormatType |format|, possibly using
// SbDecodeTargetProvider |provider|, if it is non-null.  Thus, four following
// scenarios regarding the provider may happen:
//
//   1. The provider is required by the |SbImageDecode| implementation and no
//      provider is given.  The implementation should gracefully fail by
//      immediately returning kSbDecodeTargetInvalid.
//   2. The provider is required and is passed in.  The implementation will
//      proceed forward, using the SbDecodeTarget from the provider.
//   3. The provider is not required and is passed in.  The provider will NOT be
//      called, and the implementation will proceed to decoding however it
//      desires.
//   4. The provider is not required and is not passed in.  The implementation
//      will proceed forward.
//
// Thus, it is NOT safe for clients of this API to assume that the |provider|
// it passes in will be called.  Finally, if the decode succeeds, a new
// SbDecodeTarget will be allocated. If |mime_type| image decoding for the
// requested format is not supported or the decode fails,
// kSbDecodeTargetInvalid will be returned, with any intermediate allocations
// being cleaned up in the implementation.
SB_EXPORT SbDecodeTarget SbImageDecode(
    SbDecodeTargetGraphicsContextProvider* context_provider,
    void* data,
    int data_size,
    const char* mime_type,
    SbDecodeTargetFormat format);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_IMAGE_H_
