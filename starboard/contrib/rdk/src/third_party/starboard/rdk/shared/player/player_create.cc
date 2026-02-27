//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/player.h"
#include "third_party/starboard/rdk/shared/player/player_internal.h"
#include "third_party/starboard/rdk/shared/media/gst_media_utils.h"
#include "third_party/starboard/rdk/shared/log_override.h"

SB_EXPORT SbPlayer
SbPlayerCreate(SbWindow window,
               const SbPlayerCreationParam* creation_param,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayerErrorFunc player_error_func,
               void* context,
               SbDecodeTargetGraphicsContextProvider* provider)
{
  if (!sample_deallocate_func || !decoder_status_func || !player_status_func || !player_error_func) {
    return kSbPlayerInvalid;
  }

  const auto& audio_info = creation_param->audio_stream_info;
  const auto& video_info = creation_param->video_stream_info;

  auto *max_video_capabilities = video_info.max_video_capabilities;
  auto audio_codec = audio_info.codec;
  auto video_codec = video_info.codec;
  auto drm_system  = creation_param->drm_system;
  auto output_mode = creation_param->output_mode;

  if (audio_codec == kSbMediaAudioCodecNone &&
      video_codec == kSbMediaVideoCodecNone) {
    SB_LOG(ERROR) << "SbPlayerCreate() requires at least one audio track or"
                  << " one video track.";
    return kSbPlayerInvalid;
  }

  if (audio_codec != kSbMediaAudioCodecNone &&
      !third_party::starboard::rdk::shared::media::
      GstRegistryHasElementForMediaType(audio_codec)) {
    SB_LOG(ERROR) << "Unsupported audio codec " << audio_codec;
    return kSbPlayerInvalid;
  }

  if (video_codec != kSbMediaVideoCodecNone &&
      !third_party::starboard::rdk::shared::media::
      GstRegistryHasElementForMediaType(video_codec)) {
    SB_LOG(ERROR) << "Unsupported video codec " << video_codec;
    return kSbPlayerInvalid;
  }

  if (output_mode != kSbPlayerOutputModePunchOut) {
    SB_LOG(ERROR) << "Unsupported player output mode " << output_mode;
    return kSbPlayerInvalid;
  }

  std::unique_ptr<SbPlayerPrivate> player { new SbPlayerPrivate(
      window, video_codec, audio_codec, drm_system, audio_info,
      max_video_capabilities,
      sample_deallocate_func, decoder_status_func, player_status_func,
      player_error_func, context, output_mode, provider) };

  if (!player->player_) {
    SB_LOG(ERROR) << "Failed to create player";
    return kSbPlayerInvalid;
  }

  return player.release();
}
