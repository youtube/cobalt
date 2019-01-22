// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/image.h"

#include "starboard/shared/libjpeg/jpeg_image_decoder.h"

SbDecodeTarget SbImageDecode(SbDecodeTargetGraphicsContextProvider* provider,
                             void* data,
                             int data_size,
                             const char* mime_type,
                             SbDecodeTargetFormat format) {
  if (data == NULL || !SbImageIsDecodeSupported(mime_type, format)) {
    return kSbDecodeTargetInvalid;
  }

  return starboard::shared::libjpeg::Decode(
      provider, format, static_cast<const uint8_t*>(data), data_size);
}
