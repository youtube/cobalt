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

#ifndef STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_INTERFACE_H_
#define STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_INTERFACE_H_

#include "starboard/media.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard {

// For each version V that is supported, there will be an implementation of an
// explicit specialization of the AudioDecoder class.
template <int V>
class FfmpegAudioDecoderImpl : public FfmpegAudioDecoder {
 public:
  static FfmpegAudioDecoder* Create(JobQueue* job_queue,
                                    const AudioStreamInfo& audio_stream_info);
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_FFMPEG_FFMPEG_AUDIO_DECODER_IMPL_INTERFACE_H_
