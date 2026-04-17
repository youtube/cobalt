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

// clang-format off
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
// clang-format on

// This file contains the creation of the specialized AudioDecoderImpl object
// corresponding to the version of the dynamically loaded ffmpeg library.

#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl_interface.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

// static
FfmpegAudioDecoder* FfmpegAudioDecoder::Create(
    JobQueue* job_queue,
    const AudioStreamInfo& audio_stream_info) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  if (!ffmpeg || !ffmpeg->is_valid()) {
    return NULL;
  }

  switch (ffmpeg->specialization_version()) {
    case 540:
      return FfmpegAudioDecoderImpl<540>::Create(job_queue, audio_stream_info);
    case 550:
    case 560:
      return FfmpegAudioDecoderImpl<560>::Create(job_queue, audio_stream_info);
    case 571:
      return FfmpegAudioDecoderImpl<571>::Create(job_queue, audio_stream_info);
    case 581:
      return FfmpegAudioDecoderImpl<581>::Create(job_queue, audio_stream_info);
    case 591:
      return FfmpegAudioDecoderImpl<591>::Create(job_queue, audio_stream_info);
    case 601:
      return FfmpegAudioDecoderImpl<601>::Create(job_queue, audio_stream_info);
    case 611:
      return FfmpegAudioDecoderImpl<611>::Create(job_queue, audio_stream_info);
    default:
      SB_LOG(WARNING) << "Unsupported FFMPEG specialization "
                      << ffmpeg->specialization_version();
      break;
  }
  return nullptr;
}

}  // namespace starboard
