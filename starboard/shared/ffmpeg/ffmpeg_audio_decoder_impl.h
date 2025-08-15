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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_

#include <queue>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl_interface.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_decoder_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

// Forward class declaration of the explicit specialization with value FFMPEG.
template <>
class AudioDecoderImpl<FFMPEG>;

// Declare the explicit specialization of the class with value FFMPEG.
template <>
class AudioDecoderImpl<FFMPEG> : public AudioDecoder,
                                 private starboard::player::JobQueue::JobOwner {
 public:
  explicit AudioDecoderImpl(const AudioStreamInfo& audio_stream_info);
  ~AudioDecoderImpl() override;

  // From: AudioDecoder
  static AudioDecoder* Create(const AudioStreamInfo& audio_stream_info);
  bool is_valid() const override;

  // From: starboard::player::filter::AudioDecoder
  void Initialize(const OutputCB& output_cb, const ErrorCB& error_cb) override;
  void Decode(const InputBuffers& input_buffers,
              const ConsumedCB& consumed_cb) override;
  void WriteEndOfStream() override;
  scoped_refptr<DecodedAudio> Read(int* samples_per_second) override;
  void Reset() override;

 private:
  SbMediaAudioSampleType GetSampleType() const;
  SbMediaAudioFrameStorageType GetStorageType() const;

  void InitializeCodec();
  void TeardownCodec();

  // Processes decoded (PCM) audio data received from FFmpeg. The audio data is
  // ultimately enqueued in decoded_audios_.
  void ProcessDecodedFrame(const InputBuffer& input_buffer,
                           const AVFrame& av_frame);

  static const int kMaxDecodedAudiosSize = 64;

  FFMPEGDispatch* ffmpeg_;
  OutputCB output_cb_;
  ErrorCB error_cb_;
  AVCodecContext* codec_context_;
  AVFrame* av_frame_;

  bool stream_ended_;
  std::queue<scoped_refptr<DecodedAudio>> decoded_audios_;
  AudioStreamInfo audio_stream_info_;
};

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_H_
