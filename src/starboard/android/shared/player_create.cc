// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_decoder.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "starboard/string.h"

using starboard::shared::starboard::player::filter::
    FilterBasedPlayerWorkerHandler;
using starboard::shared::starboard::player::PlayerWorker;
using starboard::android::shared::VideoDecoder;

SbPlayer SbPlayerCreate(SbWindow window,
                        const SbPlayerCreationParam* creation_param,
                        SbPlayerDeallocateSampleFunc sample_deallocate_func,
                        SbPlayerDecoderStatusFunc decoder_status_func,
                        SbPlayerStatusFunc player_status_func,
                        SbPlayerErrorFunc player_error_func,
                        void* context,
                        SbDecodeTargetGraphicsContextProvider* provider) {
  if (!creation_param) {
    SB_LOG(ERROR) << "CreationParam cannot be null.";
    return kSbPlayerInvalid;
  }

  bool has_audio =
      creation_param->audio_sample_info.codec != kSbMediaAudioCodecNone;
  bool has_video =
      creation_param->video_sample_info.codec != kSbMediaVideoCodecNone;

  const char* audio_mime =
      has_audio ? creation_param->audio_sample_info.mime : "";
  const char* video_mime =
      has_video ? creation_param->video_sample_info.mime : "";
  const char* max_video_capabilities =
      has_video ? creation_param->video_sample_info.max_video_capabilities : "";

  if (!audio_mime) {
    SB_LOG(ERROR) << "creation_param->audio_sample_info.mime cannot be null.";
    return kSbPlayerInvalid;
  }
  if (!video_mime) {
    SB_LOG(ERROR) << "creation_param->video_sample_info.mime cannot be null.";
    return kSbPlayerInvalid;
  }
  if (!max_video_capabilities) {
    SB_LOG(ERROR) << "creation_param->video_sample_info.max_video_capabilities"
                  << " cannot be null.";
    return kSbPlayerInvalid;
  }

  SB_LOG(INFO) << "SbPlayerCreate() called with audio mime \"" << audio_mime
               << "\", video mime \"" << video_mime
               << "\", and max video capabilities \"" << max_video_capabilities
               << "\".";

  if (!sample_deallocate_func || !decoder_status_func || !player_status_func ||
      !player_error_func) {
    return kSbPlayerInvalid;
  }

  auto audio_codec = creation_param->audio_sample_info.codec;
  auto video_codec = creation_param->video_sample_info.codec;

  if (audio_codec != kSbMediaAudioCodecNone &&
      audio_codec != kSbMediaAudioCodecAac &&
      audio_codec != kSbMediaAudioCodecAc3 &&
      audio_codec != kSbMediaAudioCodecEac3 &&
      audio_codec != kSbMediaAudioCodecOpus) {
    SB_LOG(ERROR) << "Unsupported audio codec " << audio_codec;
    return kSbPlayerInvalid;
  }

  if (video_codec != kSbMediaVideoCodecNone &&
      video_codec != kSbMediaVideoCodecH264 &&
      video_codec != kSbMediaVideoCodecH265 &&
      video_codec != kSbMediaVideoCodecVp9 &&
      video_codec != kSbMediaVideoCodecAv1) {
    SB_LOG(ERROR) << "Unsupported video codec " << video_codec;
    return kSbPlayerInvalid;
  }

  if (audio_codec == kSbMediaAudioCodecNone &&
      video_codec == kSbMediaVideoCodecNone) {
    SB_LOG(ERROR) << "SbPlayerCreate() requires at least one audio track or"
                  << " one video track.";
    return kSbPlayerInvalid;
  }

  if (has_audio && creation_param->audio_sample_info.number_of_channels >
                       SbAudioSinkGetMaxChannels()) {
    SB_LOG(ERROR) << "creation_param->audio_sample_info.number_of_channels"
                  << " exceeds the maximum number of audio channels supported"
                  << " by this platform.";
    return kSbPlayerInvalid;
  }

  auto output_mode = creation_param->output_mode;
  if (SbPlayerGetPreferredOutputMode(creation_param) != output_mode) {
    SB_LOG(ERROR) << "Unsupported player output mode " << output_mode;
    return kSbPlayerInvalid;
  }

  if (strlen(max_video_capabilities) == 0) {
    // Check the availability of hardware video decoder. Main player must use a
    // hardware codec, but Android doesn't support multiple concurrent hardware
    // codecs. Since it's not safe to have multiple hardware codecs, we only
    // support one main player on Android, which can be either in punch out mode
    // or decode to target mode.
    const int kMaxNumberOfHardwareDecoders = 1;
    if (VideoDecoder::number_of_hardware_decoders() >=
        kMaxNumberOfHardwareDecoders) {
      return kSbPlayerInvalid;
    }
  }

  if (creation_param->output_mode != kSbPlayerOutputModeDecodeToTexture &&
      // TODO: This is temporary for supporting background media playback.
      //       Need to be removed with media refactor.
      video_codec != kSbMediaVideoCodecNone) {
    // Check the availability of the video window. As we only support one main
    // player, and sub players are in decode to texture mode on Android, a
    // single video window should be enough.
    if (!starboard::android::shared::VideoSurfaceHolder::
            IsVideoSurfaceAvailable()) {
      SB_LOG(ERROR) << "Video surface is not available now.";
      return kSbPlayerInvalid;
    }
  }

  starboard::scoped_ptr<PlayerWorker::Handler> handler(
      new FilterBasedPlayerWorkerHandler(creation_param, provider));
  SbPlayer player = SbPlayerPrivate::CreateInstance(
      audio_codec, video_codec, &creation_param->audio_sample_info,
      sample_deallocate_func, decoder_status_func, player_status_func,
      player_error_func, context, handler.Pass());

  if (creation_param->output_mode != kSbPlayerOutputModeDecodeToTexture) {
    // TODO: accomplish this through more direct means.
    // Set the bounds to initialize the VideoSurfaceView. The initial values
    // don't matter.
    SbPlayerSetBounds(player, 0, 0, 0, 0, 0);
  }

  return player;
}
