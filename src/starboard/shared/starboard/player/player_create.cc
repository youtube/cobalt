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

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/decode_target.h"
#include "starboard/shared/media_session/playback_state.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/starboard/player/filter/filter_based_player_worker_handler.h"
#include "starboard/shared/starboard/player/player_internal.h"
#include "starboard/shared/starboard/player/player_worker.h"
#if SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)
#include "starboard/shared/starboard/player/video_dmp_writer.h"
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)

using starboard::shared::media_session::
    UpdateActiveSessionPlatformPlaybackState;
using starboard::shared::media_session::kPlaying;
using starboard::shared::starboard::player::filter::
    FilterBasedPlayerWorkerHandler;
using starboard::shared::starboard::player::PlayerWorker;

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

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
  if (!creation_param->audio_mime) {
    SB_LOG(ERROR) << "creation_param->audio_mime cannot be null.";
    return kSbPlayerInvalid;
  }
  if (!creation_param->video_mime) {
    SB_LOG(ERROR) << "creation_param->video_mime cannot be null.";
    return kSbPlayerInvalid;
  }
  if (!creation_param->max_video_capabilities) {
    SB_LOG(ERROR) << "creation_param->max_video_capabilities cannot be null.";
    return kSbPlayerInvalid;
  }

  SB_LOG(INFO) << "SbPlayerCreate() called with audio mime \""
               << creation_param->audio_mime << "\", video mime \""
               << creation_param->video_mime
               << "\", and max video capabilities \""
               << creation_param->max_video_capabilities << "\".";

  SbMediaAudioCodec audio_codec = creation_param->audio_sample_info.codec;
  SbMediaVideoCodec video_codec = creation_param->video_sample_info.codec;
  SbDrmSystem drm_system = creation_param->drm_system;
  const SbMediaAudioSampleInfo* audio_sample_info =
      &creation_param->audio_sample_info;
  const char* max_video_capabilities = creation_param->max_video_capabilities;
  const auto output_mode = creation_param->output_mode;

#else  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

SbPlayer SbPlayerCreate(SbWindow window,
                        SbMediaVideoCodec video_codec,
                        SbMediaAudioCodec audio_codec,
#if SB_API_VERSION < 10
                        SbMediaTime duration_pts,
#endif  // SB_API_VERSION < 10
                        SbDrmSystem drm_system,
                        const SbMediaAudioSampleInfo* audio_sample_info,
#if SB_API_VERSION >= 11
                        const char* max_video_capabilities,
#endif  // SB_API_VERSION >= 11
                        SbPlayerDeallocateSampleFunc sample_deallocate_func,
                        SbPlayerDecoderStatusFunc decoder_status_func,
                        SbPlayerStatusFunc player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
                        SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
                        void* context,
                        SbPlayerOutputMode output_mode,
                        SbDecodeTargetGraphicsContextProvider* provider) {
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  SB_UNREFERENCED_PARAMETER(window);
#if SB_API_VERSION >= 11
  SB_UNREFERENCED_PARAMETER(max_video_capabilities);
#endif  // SB_API_VERSION >= 11
#if SB_API_VERSION < 10
  SB_UNREFERENCED_PARAMETER(duration_pts);
#endif  // SB_API_VERSION < 10
#if SB_API_VERSION >= 11
  if (audio_sample_info) {
    SB_DCHECK(audio_sample_info->codec == audio_codec);
  }
#endif  // SB_API_VERSION >= 11

  if (!sample_deallocate_func || !decoder_status_func || !player_status_func
#if SB_HAS(PLAYER_ERROR_MESSAGE)
      || !player_error_func
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
      ) {
    return kSbPlayerInvalid;
  }

  const int64_t kDefaultBitRate = 0;
  if (audio_codec != kSbMediaAudioCodecNone &&
      !SbMediaIsAudioSupported(audio_codec, kDefaultBitRate)) {
    SB_LOG(ERROR) << "Unsupported audio codec " << audio_codec;
    return kSbPlayerInvalid;
  }

  const int kDefaultProfile = -1;
  const int kDefaultLevel = -1;
  const int kDefaultColorDepth = 8;
  const int kDefaultFrameWidth = 0;
  const int kDefaultFrameHeight = 0;
  const int kDefaultFrameRate = 0;
  if (video_codec != kSbMediaVideoCodecNone &&
      !SbMediaIsVideoSupported(video_codec,
#if SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                               kDefaultProfile, kDefaultLevel,
                               kDefaultColorDepth, kSbMediaPrimaryIdUnspecified,
                               kSbMediaTransferIdUnspecified,
                               kSbMediaMatrixIdUnspecified,
#endif  // SB_HAS(MEDIA_IS_VIDEO_SUPPORTED_REFINEMENT)
                               kDefaultFrameWidth, kDefaultFrameHeight,
                               kDefaultBitRate, kDefaultFrameRate
#if SB_API_VERSION >= 10
                               ,
                               output_mode == kSbPlayerOutputModeDecodeToTexture
#endif
                               )) {
    SB_LOG(ERROR) << "Unsupported video codec " << video_codec;
    return kSbPlayerInvalid;
  }

  if (audio_codec != kSbMediaAudioCodecNone && !audio_sample_info) {
    SB_LOG(ERROR)
        << "SbPlayerCreate() requires a non-NULL SbMediaAudioSampleInfo "
        << "when |audio_codec| is not kSbMediaAudioCodecNone";
    return kSbPlayerInvalid;
  }

  if (audio_codec == kSbMediaAudioCodecNone &&
      video_codec == kSbMediaVideoCodecNone) {
    SB_LOG(ERROR) << "SbPlayerCreate() requires at least one audio track or"
                  << " one video track.";
    return kSbPlayerInvalid;
  }

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (SbPlayerGetPreferredOutputMode(creation_param) != output_mode) {
    SB_LOG(ERROR) << "Unsupported player output mode " << output_mode;
    return kSbPlayerInvalid;
  }
#else   // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
  if (!SbPlayerOutputModeSupported(output_mode, video_codec, drm_system)) {
    SB_LOG(ERROR) << "Unsupported player output mode " << output_mode;
    return kSbPlayerInvalid;
  }
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

  UpdateActiveSessionPlatformPlaybackState(kPlaying);

  starboard::scoped_ptr<PlayerWorker::Handler> handler(
      new FilterBasedPlayerWorkerHandler(video_codec, audio_codec, drm_system,
                                         audio_sample_info, output_mode,
                                         provider));

  SbPlayer player = SbPlayerPrivate::CreateInstance(
      audio_codec, video_codec, audio_sample_info, sample_deallocate_func,
      decoder_status_func, player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
      player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
      context, handler.Pass());

#if SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)
  using ::starboard::shared::starboard::player::video_dmp::VideoDmpWriter;
  VideoDmpWriter::OnPlayerCreate(player, audio_codec, video_codec, drm_system,
                                 audio_sample_info);
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)

  return player;
}
