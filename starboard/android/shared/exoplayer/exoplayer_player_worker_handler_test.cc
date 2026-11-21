// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/exoplayer/exoplayer_player_worker_handler.h"

#include <atomic>
#include <memory>

#include "starboard/android/shared/exoplayer/exoplayer_util.h"
#include "starboard/android/shared/video_window.h"
#include "starboard/decode_target.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/player_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// A MIME that MimeType::Create() rejects, so that the ExoPlayerBridge fails to
// initialize before it needs a video surface or an ExoPlayer instance.
constexpr char kMalformedMime[] = "invalid";

SbPlayerCreationParam CreateAudioOnlyParam(const char* audio_mime) {
  SbPlayerCreationParam param = {};
  param.drm_system = kSbDrmSystemInvalid;
  param.output_mode = kSbPlayerOutputModePunchOut;
  param.audio_stream_info.codec = kSbMediaAudioCodecAac;
  param.audio_stream_info.mime = audio_mime;
  param.audio_stream_info.number_of_channels = 2;
  param.audio_stream_info.samples_per_second = 48'000;
  param.audio_stream_info.bits_per_sample = 16;
  param.video_stream_info.codec = kSbMediaVideoCodecNone;
  param.video_stream_info.mime = "";
  param.video_stream_info.max_video_capabilities = "";
  return param;
}

SbMediaVideoStreamInfo CreateVideoStreamInfo(const char* mime) {
  SbMediaVideoStreamInfo info = {};
  info.codec = kSbMediaVideoCodecVp9;
  info.mime = mime;
  info.max_video_capabilities = "";
  info.frame_width = 1280;
  info.frame_height = 720;
  info.color_metadata.bits_per_channel = 8;
  info.color_metadata.primaries = kSbMediaPrimaryIdBt709;
  info.color_metadata.transfer = kSbMediaTransferIdBt709;
  info.color_metadata.matrix = kSbMediaMatrixIdBt709;
  info.color_metadata.range = kSbMediaRangeIdLimited;
  return info;
}

// Records what a PlayerWorker reports. It is the |context| of the callbacks.
struct PlayerEvents {
  std::atomic_bool initialized = false;
  std::atomic_int error_count = 0;
};

void OnPlayerStatus(SbPlayer player,
                    void* context,
                    SbPlayerState state,
                    int ticket) {
  if (state == kSbPlayerStateInitialized) {
    static_cast<PlayerEvents*>(context)->initialized.store(true);
  }
}

void OnPlayerError(SbPlayer player,
                   void* context,
                   SbPlayerError error,
                   const char* message) {
  static_cast<PlayerEvents*>(context)->error_count.fetch_add(1);
}

// Creates a PlayerWorker backed by an ExoPlayerPlayerWorkerHandler, sends it
// the commands that StarboardRenderer queues right after SbPlayerCreate(), i.e.
// before it can learn whether the initialization succeeded, and destroys it,
// which runs the queued commands and stops the handler.
void RunPlayer(const SbPlayerCreationParam& param,
               SbDecodeTargetGraphicsContextProvider* provider,
               PlayerEvents* events) {
  // The handle is only passed back to the callbacks, and is never dereferenced.
  int fake_player_storage = 0;
  SbPlayer fake_player = reinterpret_cast<SbPlayer>(&fake_player_storage);

  PlayerWorker worker(
      param.audio_stream_info.codec, param.video_stream_info.codec,
      std::make_unique<ExoPlayerPlayerWorkerHandler>(&param, provider),
      [](int64_t, int, int, bool) {}, /*decoder_status_func=*/nullptr,
      &OnPlayerStatus, &OnPlayerError, fake_player, events);

  worker.SetBounds({0, Rect(0, 0, 0, 0)});
  worker.SetVolume(0.5);
  worker.SetPause(true);
  worker.SetPlaybackRate(1.0);
}

// The commands queued behind a failed initialization, and the teardown, must
// report the failure as a player error instead of crashing. See b/191693650
// and b/584579207.
TEST(ExoPlayerPlayerWorkerHandlerTest, CommandsAfterFailedInitDoNotCrash) {
  SbPlayerCreationParam param = CreateAudioOnlyParam(kMalformedMime);
  PlayerEvents events;

  RunPlayer(param, /*provider=*/nullptr, &events);

  EXPECT_FALSE(events.initialized.load());
  EXPECT_EQ(events.error_count.load(), 1);
}

TEST(ExoPlayerPlayerWorkerHandlerTest, AudioOnlyPlayerInitializesAndStops) {
  SbPlayerCreationParam param =
      CreateAudioOnlyParam("audio/mp4; codecs=\"mp4a.40.2\"");
  PlayerEvents events;

  RunPlayer(param, /*provider=*/nullptr, &events);

  EXPECT_TRUE(events.initialized.load());
  EXPECT_EQ(events.error_count.load(), 0);
}

// A player that rendered to the video surface must give it back and clear it,
// rather than replace it, so that the next player can use it right away.
TEST(ExoPlayerPlayerWorkerHandlerTest, VideoPlayerReleasesAndClearsSurface) {
  if (!VideoSurfaceHolder::IsVideoSurfaceAvailable()) {
    GTEST_SKIP() << "The video surface of the test activity isn't available.";
  }

  SbPlayerCreationParam param = CreateAudioOnlyParam("");
  param.video_stream_info = CreateVideoStreamInfo("");
  PlayerEvents events;
  // Counts the requests to run on the GLES thread, which is how the surface is
  // cleared, without running them.
  std::atomic_int gles_task_count = 0;
  SbDecodeTargetGraphicsContextProvider provider{
      nullptr, nullptr,
      [](SbDecodeTargetGraphicsContextProvider* provider,
         SbDecodeTargetGlesContextRunnerTarget, void*) {
        static_cast<std::atomic_int*>(provider->gles_context_runner_context)
            ->fetch_add(1);
      },
      &gles_task_count};

  RunPlayer(param, &provider, &events);

  EXPECT_TRUE(events.initialized.load());
  EXPECT_EQ(events.error_count.load(), 0);
  EXPECT_EQ(gles_task_count.load(), 1);
  EXPECT_TRUE(VideoSurfaceHolder::IsVideoSurfaceAvailable());
}

TEST(ExoPlayerUtilTest, CreateAudioFormatAcceptsEmptyMime) {
  // Progressive (non-MSE) playback has no MIME.
  SbPlayerCreationParam param = CreateAudioOnlyParam("");
  EXPECT_TRUE(CreateAudioFormat(param.audio_stream_info));
}

TEST(ExoPlayerUtilTest, CreateAudioFormatRejectsMalformedMime) {
  SbPlayerCreationParam param = CreateAudioOnlyParam(kMalformedMime);
  EXPECT_FALSE(CreateAudioFormat(param.audio_stream_info));
}

TEST(ExoPlayerUtilTest, CreateVideoFormatAcceptsEmptyMime) {
  EXPECT_TRUE(CreateVideoFormat(CreateVideoStreamInfo("")));
}

TEST(ExoPlayerUtilTest, CreateVideoFormatAcceptsMimeWithParams) {
  EXPECT_TRUE(CreateVideoFormat(CreateVideoStreamInfo(
      "video/webm; codecs=\"vp9\"; framerate=30; bitrate=1000000")));
}

TEST(ExoPlayerUtilTest, CreateVideoFormatRejectsMalformedMime) {
  EXPECT_FALSE(CreateVideoFormat(CreateVideoStreamInfo(kMalformedMime)));
}

}  // namespace
}  // namespace starboard
