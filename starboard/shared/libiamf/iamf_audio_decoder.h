// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LIBIAMF_IAMF_AUDIO_DECODER_H_
#define STARBOARD_SHARED_LIBIAMF_IAMF_AUDIO_DECODER_H_

#include <deque>
#include <queue>
#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/libiamf/iamf_decoder_utils.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
// #include "third_party/libiamf/source/code/include/IAMF_decoder.h"

namespace starboard {

class IamfAudioDecoder
    : public ::starboard::shared::starboard::player::filter::AudioDecoder,
      private starboard::player::JobQueue::JobOwner {
 public:
  explicit IamfAudioDecoder(const AudioStreamInfo& audio_stream_info);
  ~IamfAudioDecoder() override;

  bool is_valid() const;

  // AudioDecoder functions.
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  bool ConfigureDecoder(const IamfBufferInfo* info,
                        int64_t timestamp,
                        std::string* error_message) const;
  void TeardownDecoder();
  bool DecodeInternal(const scoped_refptr<InputBuffer>& input_buffer);

  SbMediaAudioSampleType GetSampleType() const;

  void ReportError(const std::string& message) const;

  const AudioStreamInfo audio_stream_info_;

  OutputCB output_cb_;
  ErrorCB error_cb_;

  // IAMF_DecoderHandle decoder_ = nullptr;
  bool stream_ended_ = false;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  bool decoder_is_configured_ = false;
  int samples_per_second_ = 0;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_LIBIAMF_IAMF_AUDIO_DECODER_H_
