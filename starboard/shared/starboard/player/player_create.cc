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

  if (!SbPlayerOutputModeSupported(output_mode, video_codec, drm_system)) {
    SB_LOG(ERROR) << "Unsupported player output mode " << output_mode;
    return kSbPlayerInvalid;
  }

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
