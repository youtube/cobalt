// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

// A set of utility functions to perform WSOLA (Waveform Similarity Based
// Overlap-Add) to modify audio playback speed without changing the pitch.
// See http://ieeexplore.ieee.org/document/319366 for more details.

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_WSOLA_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_WSOLA_INTERNAL_H_

#include <utility>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace internal {

typedef std::pair<int, int> Interval;

// Find the index of the block, within |search_block|, that is most similar
// to |target_block|. Obviously, the returned index is w.r.t. |search_block|.
// |exclude_interval| is an interval that is excluded from the search.
int OptimalIndex(const scoped_refptr<DecodedAudio>& search_block,
                 const scoped_refptr<DecodedAudio>& target_block,
                 SbMediaAudioFrameStorageType storage_type,
                 Interval exclude_interval);

// Return a "periodic" Hann window(https://en.wikipedia.org/wiki/Hann_function).
// This is the first L samples of an L+1 Hann window. It is perfect
// reconstruction for overlap-and-add.
void GetSymmetricHanningWindow(int window_length, float* window);

}  // namespace internal
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_WSOLA_INTERNAL_H_
