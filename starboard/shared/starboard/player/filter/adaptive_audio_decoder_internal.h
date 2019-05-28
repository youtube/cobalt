// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_ADAPTIVE_AUDIO_DECODER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_ADAPTIVE_AUDIO_DECODER_INTERNAL_H_

#include <memory>
#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/filter/audio_resampler.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

#if SB_API_VERSION >= SB_REFACTOR_PLAYER_SAMPLE_INFO_VERSION
class AdaptiveAudioDecoder : public AudioDecoder, private JobQueue::JobOwner {
 public:
  typedef std::function<scoped_ptr<filter::AudioDecoder>(
      const SbMediaAudioSampleInfo& audio_sample_info,
      SbDrmSystem drm_system)>
      AudioDecoderCreator;

  typedef std::function<void(SbMediaAudioSampleType* output_sample_type,
                             SbMediaAudioFrameStorageType* output_storage_type,
                             uint32_t* output_samples_per_second)>
      OutputFormatAdjustmentCallback;

  AdaptiveAudioDecoder(const SbMediaAudioSampleInfo& audio_sample_info,
                       SbDrmSystem drm_system,
                       const AudioDecoderCreator& audio_decoder_creator,
                       const OutputFormatAdjustmentCallback&
                           output_adjustment_callback = nullptr);
  ~AdaptiveAudioDecoder() override;

  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const scoped_refptr<InputBuffer>& input_buffer,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read() override;
  void Reset() override;

  SbMediaAudioSampleType GetSampleType() const override {
    SB_DCHECK(first_output_received_);
    return output_sample_type_;
  }
  SbMediaAudioFrameStorageType GetStorageType() const override {
    SB_DCHECK(first_output_received_);
    return output_storage_type_;
  }
  int GetSamplesPerSecond() const override {
    SB_DCHECK(first_output_received_);
    return output_samples_per_second_;
  }

 private:
  void ProcessOneInputBuffer(const scoped_refptr<InputBuffer>& input_buffer,
                             const ConsumedCB& consumed_cb);

  void InitializeAudioDecoder(const SbMediaAudioSampleInfo& audio_sample_info);
  void TeardownAudioDecoder();
  void OnDecoderOutput();

  const SbMediaAudioSampleInfo initilize_audio_sample_info_;
  const SbDrmSystem drm_system_;
  const AudioDecoderCreator audio_decoder_creator_;
  const OutputFormatAdjustmentCallback output_adjustment_callback_;
  SbMediaAudioSampleType output_sample_type_;
  SbMediaAudioFrameStorageType output_storage_type_;
  uint32_t output_samples_per_second_;
  SbMediaAudioSampleInfo input_audio_sample_info_ = {};

  OutputCB output_cb_ = nullptr;
  ErrorCB error_cb_ = nullptr;

  scoped_ptr<filter::AudioDecoder> audio_decoder_;
  scoped_ptr<filter::AudioResampler> resampler_;
  scoped_refptr<InputBuffer> pending_input_buffer_;
  ConsumedCB pending_consumed_cb_;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  bool flushing_ = false;
  bool stream_ended_ = false;
  bool first_output_received_ = false;
  bool output_format_checked_ = false;
};
#endif  // SB_API_VERSION >= SB_REFACTOR_PLAYER_SAMPLE_INFO_VERSION

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_ADAPTIVE_AUDIO_DECODER_INTERNAL_H_
