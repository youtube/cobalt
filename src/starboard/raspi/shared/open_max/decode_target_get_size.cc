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

#include "starboard/decode_target.h"

#include "starboard/log.h"
#include "starboard/raspi/shared/open_max/decode_target_internal.h"

void SbDecodeTargetGetSize(SbDecodeTarget decode_target,
                           int* out_width,
                           int* out_height) {
  if (SbDecodeTargetIsValid(decode_target)) {
    *out_width = decode_target->width;
    *out_height = decode_target->height;
  } else {
    SB_NOTREACHED() << "Invalid target";
  }
}
