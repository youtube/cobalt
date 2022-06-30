// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/ffmpeg/ffmpeg_demuxer.h"

#include <memory>

#include "starboard/shared/ffmpeg/ffmpeg_dispatch.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace ffmpeg {

namespace {

CobaltExtensionDemuxerStatus FFmpegDemuxer_Initialize(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->Initialize();
}

CobaltExtensionDemuxerStatus FFmpegDemuxer_Seek(int64_t seek_time_us,
                                                void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->Seek(seek_time_us);
}

SbTime FFmpegDemuxer_GetStartTime(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetStartTime();
}

SbTime FFmpegDemuxer_GetTimelineOffset(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetTimelineOffset();
}

void FFmpegDemuxer_Read(CobaltExtensionDemuxerStreamType type,
                        CobaltExtensionDemuxerReadCB read_cb,
                        void* read_cb_user_data,
                        void* user_data) {
  static_cast<FFmpegDemuxer*>(user_data)->Read(type, read_cb,
                                               read_cb_user_data);
}

bool FFmpegDemuxer_GetAudioConfig(
    CobaltExtensionDemuxerAudioDecoderConfig* config,
    void* user_data) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(user_data);
  if (!ffmpeg_demuxer->HasAudioStream()) {
    return false;
  }
  *config = ffmpeg_demuxer->GetAudioConfig();
  return true;
}

bool FFmpegDemuxer_GetVideoConfig(
    CobaltExtensionDemuxerVideoDecoderConfig* config,
    void* user_data) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(user_data);
  if (!ffmpeg_demuxer->HasVideoStream()) {
    return false;
  }
  *config = ffmpeg_demuxer->GetVideoConfig();
  return true;
}

SbTime FFmpegDemuxer_GetDuration(void* user_data) {
  return static_cast<FFmpegDemuxer*>(user_data)->GetDuration();
}

CobaltExtensionDemuxer* CreateFFmpegDemuxer(
    CobaltExtensionDemuxerDataSource* data_source,
    CobaltExtensionDemuxerAudioCodec* supported_audio_codecs,
    int64_t supported_audio_codecs_size,
    CobaltExtensionDemuxerVideoCodec* supported_video_codecs,
    int64_t supported_video_codecs_size) {
  // TODO(b/231632632): utilize supported_audio_codecs and
  // supported_video_codecs. They should ultimately be passed to FFmpegDemuxer's
  // ctor (as vectors), and the demuxer should fail fast for unsupported codecs.
  return new CobaltExtensionDemuxer{
      &FFmpegDemuxer_Initialize,
      &FFmpegDemuxer_Seek,
      &FFmpegDemuxer_GetStartTime,
      &FFmpegDemuxer_GetTimelineOffset,
      &FFmpegDemuxer_Read,
      &FFmpegDemuxer_GetAudioConfig,
      &FFmpegDemuxer_GetVideoConfig,
      &FFmpegDemuxer_GetDuration,
      FFmpegDemuxer::Create(data_source).release()};
}

void DestroyFFmpegDemuxer(CobaltExtensionDemuxer* demuxer) {
  auto* ffmpeg_demuxer = static_cast<FFmpegDemuxer*>(demuxer->user_data);
  delete ffmpeg_demuxer;
  delete demuxer;
}

const CobaltExtensionDemuxerApi kDemuxerApi = {
    /*name=*/kCobaltExtensionDemuxerApi,
    /*version=*/1,
    /*CreateDemuxer=*/&CreateFFmpegDemuxer,
    /*DestroyDemuxer=*/&DestroyFFmpegDemuxer};

FFMPEGDispatch* g_test_dispatch = nullptr;

}  // namespace

FFmpegDemuxer::FFmpegDemuxer() = default;

FFmpegDemuxer::~FFmpegDemuxer() = default;

// static
const FFMPEGDispatch* FFmpegDemuxer::GetDispatch() {
  if (g_test_dispatch) {
    return g_test_dispatch;
  }

  static const auto* const dispatch = FFMPEGDispatch::GetInstance();
  return dispatch;
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
// static
void FFmpegDemuxer::TestOnlySetFFmpegDispatch(FFMPEGDispatch* ffmpeg) {
  g_test_dispatch = ffmpeg;
}
#endif

const CobaltExtensionDemuxerApi* GetFFmpegDemuxerApi() {
  return &kDemuxerApi;
}

}  // namespace ffmpeg
}  // namespace shared
}  // namespace starboard
