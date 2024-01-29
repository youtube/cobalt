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

#include "starboard/shared/starboard/player/filter/audio_frame_discarder.h"

#include "starboard/common/log.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

void AudioFrameDiscarder::OnInputBuffers(const InputBuffers& input_buffers) {
  ScopedLock lock(mutex_);
  for (auto&& input_buffer : input_buffers) {
    SB_DCHECK(input_buffer);
    SB_DCHECK(input_buffer->sample_type() == kSbMediaTypeAudio);

    input_buffer_infos_.push({
        input_buffer->timestamp(),
        input_buffer->audio_sample_info().discarded_duration_from_front,
        input_buffer->audio_sample_info().discarded_duration_from_back,
    });
  }

  // Add a DCheck here to ensure that |input_buffer_infos_| won't grow
  // without bound, which can lead to OOM.
  SB_DCHECK(input_buffer_infos_.size() < kMaxNumberOfPendingInputBufferInfos);
}

void AudioFrameDiscarder::AdjustForDiscardedDurations(
    int sample_rate,
    scoped_refptr<DecodedAudio>* decoded_audio) {
  if (!decoded_audio || !*decoded_audio) {
    SB_LOG(ERROR) << "No input buffer to adjust.";
    SB_DCHECK(decoded_audio);
    SB_DCHECK(*decoded_audio);
    return;
  }

  InputBufferInfo input_info;
  {
    ScopedLock lock(mutex_);
    SB_DCHECK(!input_buffer_infos_.empty());

    if (input_buffer_infos_.empty()) {
      SB_LOG(WARNING)
          << "Inconsistent number of audio decoder outputs. Received "
             "outputs when input buffer list is empty.";
      return;
    }

    input_info = input_buffer_infos_.front();
    input_buffer_infos_.pop();
  }

  // We accept a small offset due to the precision of computation. If the
  // outputs have different timestamps than inputs, discarded durations will be
  // ignored.
  const int64_t kTimestampOffsetUsec = 10;
  if (std::abs(input_info.timestamp - (*decoded_audio)->timestamp()) >
      kTimestampOffsetUsec) {
    SB_LOG(WARNING) << "Inconsistent timestamps between InputBuffer (@"
                    << input_info.timestamp << ") and DecodedAudio (@"
                    << (*decoded_audio)->timestamp() << ").";
    return;
  }

  (*decoded_audio)
      ->AdjustForDiscardedDurations(sample_rate,
                                    input_info.discarded_duration_from_front,
                                    input_info.discarded_duration_from_back);
  // `(*decoded_audio)->frames()` might be 0 here.  We don't set it to nullptr
  // in this case so the DecodedAudio instance is always valid (but might be
  // empty).
}

void AudioFrameDiscarder::OnDecodedAudioEndOfStream() {
  ScopedLock lock(mutex_);
  // |input_buffer_infos_| can have extra elements when the decoder skip outputs
  // due to errors (like invalid inputs).
  SB_LOG_IF(INFO, !input_buffer_infos_.empty())
      << "OnDecodedAudioEndOfStream() is called with "
      << input_buffer_infos_.size() << " input buffer infos pending.";
}

void AudioFrameDiscarder::Reset() {
  ScopedLock lock(mutex_);
  input_buffer_infos_ = std::queue<InputBufferInfo>();
}

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
