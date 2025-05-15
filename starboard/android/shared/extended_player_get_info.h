// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STABOARD_ANDROID_SHARED_EXTENDED_PLAYER_GET_INFO_H_
#define STABOARD_ANDROID_SHARED_EXTENDED_PLAYER_GET_INFO_H_

#include "starboard/extension/extended_player_info.h"

#include "starboard/player.h"

namespace starboard {
namespace android {
namespace shared {

void PlayerGetInfo(SbPlayer player,
                   StarboardExtensionExtendedPlayerInfo* out_player_info);

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STABOARD_ANDROID_SHARED_EXTENDED_PLAYER_GET_INFO_H_
