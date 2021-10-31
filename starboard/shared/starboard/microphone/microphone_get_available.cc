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

#include "starboard/microphone.h"

#include "starboard/memory.h"
#include "starboard/shared/starboard/microphone/microphone_internal.h"

#if SB_API_VERSION < 12 && !SB_HAS(MICROPHONE)
#error "SB_HAS_MICROPHONE must be set to build this file before Starboard API \
version 12."
#endif

int SbMicrophoneGetAvailable(SbMicrophoneInfo* out_info_array,
                             int info_array_size) {
  if (out_info_array && info_array_size > 0) {
    memset(out_info_array, 0, sizeof(SbMicrophoneInfo) * info_array_size);
  }
  return SbMicrophonePrivate::GetAvailableMicrophones(out_info_array,
                                                      info_array_size);
}
