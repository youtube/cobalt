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

#ifndef STARBOARD_SHARED_OPUS_OPUS_AUDIO_DECODER_H_
#define STARBOARD_SHARED_OPUS_OPUS_AUDIO_DECODER_H_

#include <queue>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/opus/include/opus.h"

namespace starboard {
namespace shared {
namespace opus {

class OpusAudioDecoder
    : public ::starboard::shared::starboard::player::filter::AudioDecoder,
      private starboard::player::JobQueue::JobOwner {
 public:
  explicit OpusAudioDecoder(const SbMediaAudioHeader& audio_header);
  ~OpusAudioDecoder() override;

  bool is_valid() const;

  // AudioDecoder functions
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read() override;
  void Reset() override;
  SbMediaAudioSampleType GetSampleType() const override;
  SbMediaAudioFrameStorageType GetStorageType() const override;
  int GetSamplesPerSecond() const override;

 private:
  OutputCB output_cb_;
  ErrorCB error_cb_;

  OpusDecoder* decoder_ = NULL;
  bool stream_ended_ = false;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  SbMediaAudioHeader audio_header_;
  std::vector<uint8_t> working_buffer_;
};

}  // namespace opus
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_OPUS_OPUS_AUDIO_DECODER_H_
