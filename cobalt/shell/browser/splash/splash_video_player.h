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

#ifndef COBALT_SHELL_BROWSER_SPLASH_SPLASH_VIDEO_PLAYER_H_
#define COBALT_SHELL_BROWSER_SPLASH_SPLASH_VIDEO_PLAYER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "starboard/player.h"
#include "starboard/window.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

// Proof of concept: plays a WebM video directly with a Starboard player,
// without a WebContents or the media pipeline. The video is demuxed up front
// with media::WebMStreamParser; only its video track is played.
//
// The player renders in punch-out mode where the platform supports it: the
// video shows wherever the UI above it is transparent.
//
// Lives on the UI thread.
class SplashVideoPlayer {
 public:
  enum class Result {
    kEnded,
    kDemuxFailed,
    kNoVideo,
    kUnsupported,
    kPlayerError,
    kTimedOut,
  };
  static const char* ResultToString(Result result);

  struct Callbacks {
    Callbacks();
    Callbacks(Callbacks&&);
    Callbacks& operator=(Callbacks&&);
    ~Callbacks();

    // The first frame is being presented.
    base::OnceClosure on_presenting;
    // Playback is over, successfully or not. The player has been destroyed.
    base::OnceCallback<void(Result)> on_finished;
  };

  SplashVideoPlayer();
  SplashVideoPlayer(const SplashVideoPlayer&) = delete;
  SplashVideoPlayer& operator=(const SplashVideoPlayer&) = delete;
  ~SplashVideoPlayer();

  // Plays |webm| in |bounds| of |window|. Runs the callbacks asynchronously,
  // unless this is destroyed first. Playback stops after |max_duration|.
  void Play(const std::vector<uint8_t>& webm,
            SbWindow window,
            const gfx::Rect& bounds,
            base::TimeDelta max_duration,
            Callbacks callbacks);

  // Stops playback, without running |on_finished|.
  void Stop();

 private:
  struct Sample {
    std::vector<uint8_t> data;
    int64_t timestamp_us;
    bool is_key_frame;
  };

  bool Demux(const std::vector<uint8_t>& webm);
  void Finish(Result result);
  void WriteSamples();

  // Starboard callbacks, on Starboard threads.
  static void DecoderStatusCB(SbPlayer player,
                              void* context,
                              SbMediaType type,
                              SbPlayerDecoderState state,
                              int ticket);
  static void PlayerStatusCB(SbPlayer player,
                             void* context,
                             SbPlayerState state,
                             int ticket);
  static void PlayerErrorCB(SbPlayer player,
                            void* context,
                            SbPlayerError error,
                            const char* message);
  static void DeallocateSampleCB(SbPlayer player,
                                 void* context,
                                 const void* sample_buffer);

  // Their counterparts on the UI thread.
  void OnDecoderNeedsData(SbMediaType type, int ticket);
  void OnPlayerStatus(SbPlayerState state, int ticket);
  void OnPlayerError(SbPlayerError error, std::string message);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  SbPlayer player_ = kSbPlayerInvalid;
  int ticket_ = SB_PLAYER_INITIAL_TICKET;

  // The video track.
  SbMediaVideoStreamInfo video_stream_info_ = {};
  std::string mime_;
  std::string max_video_capabilities_;
  std::vector<Sample> samples_;
  size_t next_sample_ = 0;
  bool end_of_stream_written_ = false;

  Callbacks callbacks_;
  base::OneShotTimer timeout_;
  base::WeakPtr<SplashVideoPlayer> weak_this_;
  base::WeakPtrFactory<SplashVideoPlayer> weak_factory_{this};
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SPLASH_VIDEO_PLAYER_H_
