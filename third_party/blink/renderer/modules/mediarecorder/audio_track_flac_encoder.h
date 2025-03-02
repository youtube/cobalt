// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_FLAC_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_FLAC_ENCODER_H_

#include <memory>

#include "media/base/audio_bus.h"
// #include "media/base/audio_converter.h"
#include "media/base/audio_fifo.h"
#include "media/base/audio_parameters.h"
#include "third_party/blink/renderer/modules/mediarecorder/audio_track_encoder.h"

#include "third_party/flac/include/FLAC/stream_encoder.h"

namespace blink {

// Class encapsulating FLAC-related encoding details. It contains an
// AudioConverter to adapt incoming data to the format FLAC likes to have.
class AudioTrackFlacEncoder : public AudioTrackEncoder,
                              public media::AudioConverter::InputCallback {
 public:
  AudioTrackFlacEncoder(OnEncodedAudioCB on_encoded_audio_cb, int sample_rate = 48000);
  ~AudioTrackFlacEncoder() override;

  AudioTrackFlacEncoder(const AudioTrackFlacEncoder&) = delete;
  AudioTrackFlacEncoder& operator=(const AudioTrackFlacEncoder&) = delete;

  void OnSetFormat(const media::AudioParameters& params) override;
  void EncodeAudio(std::unique_ptr<media::AudioBus> input_bus,
                   base::TimeTicks capture_time) override;

 private:
  bool is_initialized() const { return !!flac_encoder_; }

  void DestroyExistingFlacEncoder();

  // media::AudioConverted::InputCallback implementation.
  double ProvideInput(media::AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const media::AudioGlitchInfo& glitch_info) override;

  // FLAC encoder callback.
  static FLAC__StreamEncoderWriteStatus WriteCallback(
      const FLAC__StreamEncoder* encoder, const FLAC__byte buffer[],
      size_t bytes, unsigned int samples, unsigned int current_frame,
      void* client_data);

  // THREAD_CHECKER(thread_checker_);
  // FLAC encoder.
  FLAC__StreamEncoder* flac_encoder_;
  // Cached encoded data.
  std::string encoded_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_AUDIO_TRACK_FLAC_ENCODER_H_
