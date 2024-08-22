// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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
//     SbDecodeTarget result_target = SbImageDecode(provider, data, data_size,
//                                                  mime_type, format);
//

#ifndef STARBOARD_IMAGE_H_
#define STARBOARD_IMAGE_H_

#error This file is deprecated with SB_API_VERSION 16.

#endif  // STARBOARD_IMAGE_H_
