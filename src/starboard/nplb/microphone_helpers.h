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

#ifndef STARBOARD_NPLB_MICROPHONE_HELPERS_H_
#define STARBOARD_NPLB_MICROPHONE_HELPERS_H_

#include "starboard/microphone.h"

#if SB_HAS(MICROPHONE) && SB_VERSION(2)

namespace starboard {
namespace nplb {
const int kMaxNumberOfMicrophone = 20;
const int kBufferSize = 32 * 1024;
const int kNormallyUsedSampleRateInHz = 16000;
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(MICROPHONE) && SB_VERSION(2)

#endif  // STARBOARD_NPLB_MICROPHONE_HELPERS_H_
