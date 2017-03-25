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

#include "starboard/tizen/shared/ffmpeg/ffmpeg_common.h"

#include "starboard/log.h"
#include "starboard/once.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

SbOnceControl ffmpeg_initialization_once = SB_ONCE_INITIALIZER;

}  // namespace

void InitializeFfmpeg() {
  bool initialized = SbOnce(&ffmpeg_initialization_once, av_register_all);
  SB_DCHECK(initialized);
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
