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

// This file contains the creation of the specialized AudioDecoderImpl object
// corresponding to the version of the linked ffmpeg library.

#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder.h"

#include "starboard/player.h"
#include "starboard/shared/ffmpeg/ffmpeg_audio_decoder_impl.h"
#include "starboard/shared/ffmpeg/ffmpeg_common.h"
#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/shared/starboard/media/media_util.h"

namespace starboard::shared::ffmpeg {

// static
AudioDecoder* AudioDecoder::Create(const AudioStreamInfo& audio_stream_info) {
  FFMPEGDispatch* ffmpeg = FFMPEGDispatch::GetInstance();
  SB_DCHECK(ffmpeg && ffmpeg->is_valid());
  SB_DCHECK(FFMPEG == ffmpeg->specialization_version());

  return AudioDecoderImpl<FFMPEG>::Create(audio_stream_info);
}

}  // namespace starboard::shared::ffmpeg
