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

#include "starboard/shared/win32/decode_target_internal.h"

#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/memory.h"

SbDecodeTargetPrivate::SbDecodeTargetPrivate() : refcount(1) {
  SbMemorySet(&info, 0, sizeof(info));
}

void SbDecodeTargetPrivate::AddRef() {
  SbAtomicBarrier_Increment(&refcount, 1);
}

void SbDecodeTargetPrivate::Release() {
  int new_count = SbAtomicBarrier_Increment(&refcount, -1);
  SB_DCHECK(new_count >= 0);
  if (new_count == 0) {
    delete this;
  }
}

void SbDecodeTargetRelease(SbDecodeTarget decode_target) {
  if (SbDecodeTargetIsValid(decode_target)) {
    decode_target->Release();
  }
}

SbDecodeTargetFormat SbDecodeTargetGetFormat(SbDecodeTarget decode_target) {
  // Note that kSbDecodeTargetFormat2PlaneYUVNV12 represents DXGI_FORMAT_NV12.
  SB_DCHECK(kSbDecodeTargetFormat2PlaneYUVNV12 ==
            decode_target->info.format);
  return decode_target->info.format;
}

bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                           SbDecodeTargetInfo* out_info) {
  if (!out_info || !SbMemoryIsZero(out_info, sizeof(*out_info))) {
    SB_DCHECK(false) << "out_info must be zeroed out.";
    return false;
  }
  SbMemoryCopy(out_info, &decode_target->info, sizeof(*out_info));
  return true;
}
