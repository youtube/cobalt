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

#include "starboard/decode_target.h"

#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/shared/starboard/decode_target/decode_target_internal.h"

bool SbDecodeTargetGetInfo(SbDecodeTarget decode_target,
                           SbDecodeTargetInfo* out_info) {
  SB_DCHECK(SbDecodeTargetIsValid(decode_target));

  if (!starboard::common::MemoryIsZero(out_info, sizeof(*out_info))) {
    SB_DCHECK(false) << "out_info must be zeroed out.";
    return false;
  }

  return decode_target->GetInfo(out_info);
}
