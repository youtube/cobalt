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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_DISCARDER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_DISCARDER_H_

#include <queue>

#include "starboard/common/mutex.h"
#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// The class helps tracking the information contained in the InputBuffer objects
// required to adjust discarded duration on the DecodedAudio object.  It should
// be used when the DecodedAudio object is produced asynchronously, and the
// corresponding InputBuffer object isn't available at the time.
// This class assumes that there is exact one DecodedAudio object produced for
// one InputBuffer object, which may not always be the case.
class AudioFrameDiscarder {
 public:
  void OnInputBuffers(const InputBuffers& input_buffers);
  void AdjustForDiscardedDurations(int sample_rate,
                                   scoped_refptr<DecodedAudio>* decoded_audio);
  void OnDecodedAudioEndOfStream();

  void Reset();

 private:
  struct InputBufferInfo {
    SbTime timestamp;
    SbTime discarded_duration_from_front;
    SbTime discarded_duration_from_back;
  };

  static constexpr size_t kMaxNumberOfPendingInputBufferInfos = 128;

  Mutex mutex_;
  std::queue<InputBufferInfo> input_buffer_infos_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_AUDIO_FRAME_DISCARDER_H_
