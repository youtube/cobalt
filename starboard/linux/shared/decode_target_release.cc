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
#include "starboard/linux/shared/decode_target_internal.h"

void SbDecodeTargetRelease(SbDecodeTarget decode_target) {
  // Most of the actual data within |decode_target| is stored in the reference
  // counted decode_target->data, so deleting |decode_target| here may not
  // actually release any resources, if there are other references to
  // decode_target->data.
  delete decode_target;
}
