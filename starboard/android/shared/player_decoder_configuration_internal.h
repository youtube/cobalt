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

#ifndef STARBOARD_ANDROID_SHARED_PLAYER_DECODER_CONFIGURATION_INTERNAL_H_
#define STARBOARD_ANDROID_SHARED_PLAYER_DECODER_CONFIGURATION_INTERNAL_H_

#include <optional>

namespace starboard::android::shared {

// Get initial_max_frames_in_decoder via
// SetVideoInitialMaxFramesInDecoderForCurrentThread().
std::optional<int> GetVideoInitialMaxFramesInDecoderForCurrentThread();

// Specifies the initial max frames in video decoder.
// |initial_max_frames_in_decoder| should be non-negative value.
void SetVideoInitialMaxFramesInDecoderForCurrentThread(
    int initial_max_frames_in_decoder);

// Get max_pending_input_frames via
// SetVideoMaxPendingInputFramesForCurrentThread().
std::optional<int> GetVideoMaxPendingInputFramesForCurrentThread();

// Specifies the max pending video input frames.
// |max_pending_input_frames| should be non-negative value.
void SetVideoMaxPendingInputFramesForCurrentThread(
    int max_pending_input_frames);

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_PLAYER_DECODER_CONFIGURATION_INTERNAL_H_
