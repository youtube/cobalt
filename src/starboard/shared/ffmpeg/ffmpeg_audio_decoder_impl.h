// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// For each version V that is supported, there will be an implementation of an
// explicit specialization of the AudioDecoder class.
template <int V>
class AudioDecoderImpl : public AudioDecoder {
 public:
  static AudioDecoder* Create(SbMediaAudioCodec audio_codec,
                              const SbMediaAudioHeader& audio_header);
};

// Forward class declaration of the explicit specialization with value FFMPEG.
template <>
class AudioDecoderImpl<FFMPEG>;

// Declare the explicit specialization of the class with value FFMPEG.
template <>
class AudioDecoderImpl<FFMPEG> : public AudioDecoder,
                                 private starboard::player::JobQueue::JobOwner {
 public:
  AudioDecoderImpl(SbMediaAudioCodec audio_codec,
                   const SbMediaAudioHeader& audio_header);
  ~AudioDecoderImpl() override;

  // From: AudioDecoder
  static AudioDecoder* Create(SbMediaAudioCodec audio_codec,
                              const SbMediaAudioHeader& audio_header);
  bool is_valid() const override;

  // From: starboard::player::filter::AudioDecoder
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
  void InitializeCodec();
  void TeardownCodec();

  static const int kMaxDecodedAudiosSize = 64;

  FFMPEGDispatch* ffmpeg_;
  OutputCB output_cb_;
  ErrorCB error_cb_;
  SbMediaAudioCodec audio_codec_;
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio> > decoded_audios_;
  SbMediaAudioHeader audio_header_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_
