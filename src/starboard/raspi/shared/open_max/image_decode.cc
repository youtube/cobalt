// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/mutex.h"
#include "starboard/raspi/shared/open_max/open_max_image_decode_component.h"

namespace open_max = starboard::raspi::shared::open_max;

namespace {
starboard::Mutex decode_lock_;
}

SbDecodeTarget SbImageDecode(SbDecodeTargetGraphicsContextProvider* provider,
                             void* data,
                             int data_size,
                             const char* mime_type,
                             SbDecodeTargetFormat format) {
  SB_CHECK(data);
  SB_DCHECK(SbImageIsDecodeSupported(mime_type, format));
  SbDecodeTarget target = kSbDecodeTargetInvalid;

  // Only one image can be decoded at a time by the component.
  {
    starboard::ScopedLock lock(decode_lock_);
    open_max::OpenMaxImageDecodeComponent decoder;
    target = decoder.Decode(provider, mime_type, format, data, data_size);
  }

  return target;
}
