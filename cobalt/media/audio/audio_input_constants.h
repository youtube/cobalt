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

#ifndef COBALT_MEDIA_AUDIO_AUDIO_INPUT_CONSTANTS_H_
#define COBALT_MEDIA_AUDIO_AUDIO_INPUT_CONSTANTS_H_

namespace cobalt {
namespace media {

// Hardcoded constants for the fast-track microphone capture path.
constexpr int kSampleRate = 16'000;
constexpr int kSamplesPerBuffer = 128;

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_AUDIO_AUDIO_INPUT_CONSTANTS_H_
