// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/player.h"

#include "starboard/common/log.h"

namespace starboard {

const char* GetPlayerOutputModeName(SbPlayerOutputMode output_mode) {
  switch (output_mode) {
    case kSbPlayerOutputModeDecodeToTexture:
      return "decode-to-texture";
    case kSbPlayerOutputModePunchOut:
      return "punch-out";
    case kSbPlayerOutputModeInvalid:
      return "invalid";
  }

  SB_NOTREACHED();
  return "invalid";
}

const char* GetPlayerStateName(SbPlayerState state) {
  switch (state) {
    case kSbPlayerStateInitialized:
      return "kSbPlayerStateInitialized";
    case kSbPlayerStatePrerolling:
      return "kSbPlayerStatePrerolling";
    case kSbPlayerStatePresenting:
      return "kSbPlayerStatePresenting";
    case kSbPlayerStateEndOfStream:
      return "kSbPlayerStateEndOfStream";
    case kSbPlayerStateDestroyed:
      return "kSbPlayerStateDestroyed";
  }

  SB_NOTREACHED();
  return "invalid";
}

}  // namespace starboard
