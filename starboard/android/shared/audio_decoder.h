// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_
#define STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaError.h>
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaFormat.h>

#include <queue>

#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"

namespace starboard {
namespace android {
namespace shared {

class AudioDecoder
    : public ::starboard::shared::starboard::player::filter::AudioDecoder {
 public:
  typedef ::starboard::shared::starboard::player::DecodedAudio DecodedAudio;
  typedef ::starboard::shared::starboard::player::InputBuffer InputBuffer;

  AudioDecoder(SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header);
  ~AudioDecoder() SB_OVERRIDE;

  void Decode(const InputBuffer& input_buffer) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  scoped_refptr<DecodedAudio> Read() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;

  SbMediaAudioSampleType GetSampleType() const SB_OVERRIDE {
    return sample_type_;
  }
  int GetSamplesPerSecond() const SB_OVERRIDE {
    return audio_header_.samples_per_second;
  }
  bool is_valid() const {
    return media_codec_ != NULL && media_format_ != NULL;
  }

 private:
  void InitializeCodec();
  void TeardownCodec();

  AMediaCodec* media_codec_;
  AMediaFormat* media_format_;

  SbMediaAudioSampleType sample_type_;

  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  SbMediaAudioHeader audio_header_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_AUDIO_DECODER_H_
