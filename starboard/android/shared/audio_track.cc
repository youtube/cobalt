// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/audio_track.h"

#include <memory>
#include <optional>

#include "starboard/android/shared/audio_track_bridge.h"

namespace starboard {

// static
std::unique_ptr<AudioTrack> AudioTrack::Create(
    SbMediaAudioCodingType coding_type,
    std::optional<SbMediaAudioSampleType> sample_type,
    int channels,
    int sampling_frequency_hz,
    int preferred_buffer_size_in_bytes,
    std::optional<int> tunnel_mode_audio_session_id,
    bool is_web_audio) {
  return AudioTrackBridge::Create(coding_type, sample_type, channels,
                                  sampling_frequency_hz,
                                  preferred_buffer_size_in_bytes,
                                  tunnel_mode_audio_session_id, is_web_audio);
}

}  // namespace starboard
