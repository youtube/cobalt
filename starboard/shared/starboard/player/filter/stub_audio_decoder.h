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

#include <memory>
#include <mutex>
#include <optional>
#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/shared/starboard/player/job_thread.h"

namespace starboard {

class StubAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  explicit StubAudioDecoder(const AudioStreamInfo& audio_stream_info);
  ~StubAudioDecoder() { Reset(); }

  void Initialize(OutputCB output_cb, ErrorCB error_cb) override;

  void Decode(const InputBuffers& input_buffer,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  void DecodeBuffers(const InputBuffers& input_buffers,
                     const ConsumedCB& consumed_cb);
  void DecodeOneBuffer(const scoped_refptr<InputBuffer>& input_buffer);
  void DecodeEndOfStream();

  const SbMediaAudioCodec codec_;
  const int number_of_channels_;
  const int samples_per_second_;
  const SbMediaAudioSampleType sample_type_;

  OutputCB output_cb_;
  ErrorCB error_cb_;

  std::unique_ptr<JobThread> decoder_thread_;
  std::mutex decoded_audios_mutex_;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  scoped_refptr<InputBuffer> last_input_buffer_;
  std::optional<int> frames_per_input_;
  // Used to determine when to send multiple DecodedAudios in DecodeOneBuffer().
  int total_input_count_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_STUB_AUDIO_DECODER_H_
