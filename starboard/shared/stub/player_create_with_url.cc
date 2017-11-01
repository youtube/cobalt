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

#include "starboard/player.h"

#if SB_HAS(PLAYER_WITH_URL)

SbPlayer SbPlayerCreateWithUrl(
    const char* /* url */,
    SbWindow /* window */,
    SbMediaTime /* duration_pts */,
    SbPlayerStatusFunc /* player_status_func */,
    SbPlayerEncryptedMediaInitDataEncounteredCB /* cb */,
    void* /* context */) {
  // Stub.
  return kSbPlayerInvalid;
}

// TODO: Actually move this to a separate file or get rid of these URL
// player stubs altogether.
void SbPlayerSetDrmSystem(SbPlayer player, SbDrmSystem drm_system) { /* Stub */
}

#endif  // SB_HAS(PLAYER_WITH_URL)
