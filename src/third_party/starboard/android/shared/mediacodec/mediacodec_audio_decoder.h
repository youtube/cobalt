// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_AUDIO_DECODER_H_
#define STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_AUDIO_DECODER_H_

#include <vector>

#include "starboard/media.h"
#include "third_party/starboard/android/shared/mediacodec/mediacodec_common.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/log.h"

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaExtractor.h>
#include "starboard/file.h"

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

namespace starboard {
namespace shared {
namespace mediacodec {

class AudioDecoder : public starboard::player::filter::AudioDecoder {
 public:
  typedef starboard::player::InputBuffer InputBuffer;

  AudioDecoder(SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header);
  ~AudioDecoder() SB_OVERRIDE;

//  void Decode(const InputBuffer& input_buffer,
//              std::vector<float>* output) SB_OVERRIDE;
  void Decode(const InputBuffer& input_buffer,
              std::vector<uint8_t>* output) SB_OVERRIDE;
  void WriteEndOfStream() SB_OVERRIDE;
  void Reset() SB_OVERRIDE;
  int GetSamplesPerSecond() SB_OVERRIDE;

  bool is_valid() const { return true; }

 private:
  AMediaCodec* mediacodec_;
  AMediaFormat* mediaformat_;

  SbFile file_;

  void InitializeCodec();
  void TeardownCodec();

  bool stream_ended_;
  SbMediaAudioHeader audio_header_;
};

}  // namespace mediacodec
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_MEDIACODEC_MEDIACODEC_AUDIO_DECODER_H_
