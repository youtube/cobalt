// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_LINUX_SHARED_DECODE_TARGET_INTERNAL_H_
#define STARBOARD_LINUX_SHARED_DECODE_TARGET_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/player/filter/cpu_video_frame.h"

struct SbDecodeTargetPrivate {
  class Data : public starboard::RefCounted<Data> {
   public:
    Data() {}

    SbDecodeTargetInfo info;

   private:
    friend class starboard::RefCounted<Data>;
    ~Data();
  };

  starboard::scoped_refptr<Data> data;
};

namespace starboard {
namespace shared {

// Outputs a video frame into a SbDecodeTarget.
SbDecodeTarget DecodeTargetCreate(
    SbDecodeTargetGraphicsContextProvider* provider,
    scoped_refptr<starboard::player::filter::CpuVideoFrame> frame,
    // Possibly valid structure to reuse, instead of allocating a new object.
    SbDecodeTarget decode_target);

void DecodeTargetRelease(SbDecodeTargetGraphicsContextProvider*
                             decode_target_graphics_context_provider,
                         SbDecodeTarget decode_target);

SbDecodeTarget DecodeTargetCopy(SbDecodeTarget decode_target);

}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_LINUX_SHARED_DECODE_TARGET_INTERNAL_H_
