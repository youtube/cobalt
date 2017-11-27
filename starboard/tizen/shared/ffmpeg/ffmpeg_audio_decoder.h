// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
#define STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_

#include <queue>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/tizen/shared/ffmpeg/ffmpeg_common.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

class AudioDecoder : public starboard::player::filter::AudioDecoder {
 public:
  typedef starboard::player::DecodedAudio DecodedAudio;
  typedef starboard::player::InputBuffer InputBuffer;

  AudioDecoder(SbMediaAudioCodec audio_codec,
               const SbMediaAudioHeader& audio_header);
  ~AudioDecoder() override;

  void Decode(const InputBuffer& input_buffer) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read() override;
  void Reset() override;
  SbMediaAudioSampleType GetSampleType() const override;
  int GetSamplesPerSecond() const override;
  bool CanAcceptMoreData() const override {
    return !stream_ended_ && decoded_audios_.size() <= kMaxDecodedAudiosSize;
  }

  bool is_valid() const { return codec_context_ != NULL; }

 private:
  void InitializeCodec();
  void TeardownCodec();

  static const int kMaxDecodedAudiosSize = 64;

  SbMediaAudioCodec audio_codec_;
  SbMediaAudioSampleType sample_type_;
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  SbMediaAudioHeader audio_header_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_TIZEN_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_H_
