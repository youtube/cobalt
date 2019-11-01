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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_AUDIO_DECODER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_AUDIO_DECODER_H_

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

class StubAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  StubAudioDecoder(SbMediaAudioCodec audio_codec,
                   const SbMediaAudioSampleInfo& audio_sample_info);

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;

  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override;

  void WriteEndOfStream() override;

  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;

  void Reset() override;

 private:
  OutputCB output_cb_;
  SbMediaAudioSampleType sample_type_;
  SbMediaAudioCodec audio_codec_;
  SbMediaAudioSampleInfo audio_sample_info_;
  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  scoped_refptr<InputBuffer> last_input_buffer_;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_AUDIO_DECODER_H_
