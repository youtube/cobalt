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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_JNI_CONSTANTS_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_JNI_CONSTANTS_H_

namespace starboard::android::shared {

// This value should match `NULL_OPT_TUNNEL_SESSION_ID` in
// `AudioTrackBridge.java`.
// This value represents std::nullopt for `tunnel_mode_audio_session_id`.
constexpr int kNullOptTunnelModeAudioSessionId = -1;

}  // namespace starboard::android::shared

#endif
