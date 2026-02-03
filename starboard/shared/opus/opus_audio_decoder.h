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

#include <deque>
#include <queue>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "third_party/opus/src/include/opus_multistream.h"

namespace starboard {

class OpusAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  OpusAudioDecoder(JobQueue* job_queue,
                   const AudioStreamInfo& audio_stream_info);
  ~OpusAudioDecoder() override;

  bool is_valid() const;

  // AudioDecoder functions
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  static constexpr int kMinimumBuffersToDecode = 2;

  void InitializeCodec();
  void TeardownCodec();
  void DecodePendingBuffers();
  bool DecodeInternal(const InputBuffer& input_buffer);
  static const int kMaxOpusFramesPerAU = 9600;

  SbMediaAudioSampleType GetSampleType() const;

  OutputCB output_cb_;
  ErrorCB error_cb_;

  OpusMSDecoder* decoder_ = NULL;
  bool stream_ended_ = false;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  AudioStreamInfo audio_stream_info_;
  int frames_per_au_ = kMaxOpusFramesPerAU;

  std::deque<scoped_refptr<InputBuffer>> pending_audio_buffers_;
  ConsumedCB consumed_cb_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_OPUS_OPUS_AUDIO_DECODER_H_
