// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CALLBACK_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CALLBACK_H_

#include <functional>
#include <string>

#include "starboard/player.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

#if SB_HAS(PLAYER_ERROR_MESSAGE)
typedef std::function<void(SbPlayerError error,
                           const std::string& error_message)>
    ErrorCB;
#else   // SB_HAS(PLAYER_ERROR_MESSAGE)
typedef std::function<void()> ErrorCB;
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
typedef std::function<void()> PrerolledCB;
typedef std::function<void()> EndedCB;

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CALLBACK_H_
