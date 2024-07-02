// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_PLAYER_PERF_H_
#define STARBOARD_ANDROID_SHARED_PLAYER_PERF_H_

#include "starboard/android/shared/audio_track_bridge.h"

namespace starboard {
namespace android {
namespace shared {

const void* GetPlayerPerfApi();

void SetAudioTrackBridge(AudioTrackBridge* audio_track_bridge);
void SetMediaVideoDecoderCodec(SbMediaVideoCodec codec);
void SetMediaAudioDecoderCodec(SbMediaAudioCodec codec);
void AddThreadIDInternal(const char* thread_name, int32_t thread_id);

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_PLAYER_PERF_H_
