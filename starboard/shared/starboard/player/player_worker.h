// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/closure.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/job_queue.h"
#include "starboard/thread.h"
#include "starboard/time.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// This class creates a thread that executes events posted to an internally
// created queue. This guarantees that all such events are processed on the same
// thread.
//
// This class serves as the base class for platform specific PlayerWorkers so
// they needn't maintain the thread and queue internally.
class PlayerWorker {
 public:
  class Host {
   public:
    virtual void UpdateMediaTime(SbMediaTime media_time, int ticket) = 0;
    virtual void UpdateDroppedVideoFrames(int dropped_video_frames) = 0;

   protected:
    ~Host() {}
  };

  struct Bounds {
    int x;
    int y;
    int width;
    int height;
  };

  // All functions of this class will be called from the JobQueue thread.
  class Handler {
   public:
    typedef void (PlayerWorker::*UpdateMediaTimeCB)(SbMediaTime media_time);
    typedef SbPlayerState (PlayerWorker::*GetPlayerStateCB)() const;
    typedef void (PlayerWorker::*UpdatePlayerStateCB)(
        SbPlayerState player_state);

    virtual ~Handler() {}

    // All the following functions return false to signal a fatal error.  The
    // event processing loop in PlayerWorker will termimate in this case.
    virtual bool Init(PlayerWorker* player_worker,
                      JobQueue* job_queue,
                      SbPlayer player,
                      UpdateMediaTimeCB update_media_time_cb,
                      GetPlayerStateCB get_player_state_cb,
                      UpdatePlayerStateCB update_player_state_cb) = 0;
    virtual bool Seek(SbMediaTime seek_to_pts, int ticket) = 0;
    virtual bool WriteSample(InputBuffer input_buffer, bool* written) = 0;
    virtual bool WriteEndOfStream(SbMediaType sample_type) = 0;
    virtual bool SetPause(bool pause) = 0;
#if SB_API_VERSION >= 4
    virtual bool SetPlaybackRate(double playback_rate) = 0;
#endif  // SB_API_VERSION >= 4
    virtual bool SetBounds(const Bounds& bounds) = 0;

    // Once this function returns, all processing on the Handler and related
    // objects has to be stopped.  The JobQueue will be destroyed immediately
    // after and is no longer safe to access.
    virtual void Stop() = 0;

#if SB_API_VERSION >= 4
    virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
#endif  // SB_API_VERSION >= 4
  };

  PlayerWorker(Host* host,
               scoped_ptr<Handler> handler,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayer player,
               void* context);
  ~PlayerWorker();

  void Seek(SbMediaTime seek_to_pts, int ticket) {
    job_queue_->Schedule(
        Bind(&PlayerWorker::DoSeek, this, seek_to_pts, ticket));
  }

  void WriteSample(InputBuffer input_buffer) {
    job_queue_->Schedule(
        Bind(&PlayerWorker::DoWriteSample, this, input_buffer));
  }

  void WriteEndOfStream(SbMediaType sample_type) {
    job_queue_->Schedule(
        Bind(&PlayerWorker::DoWriteEndOfStream, this, sample_type));
  }

#if SB_API_VERSION >= 4 || SB_IS(PLAYER_PUNCHED_OUT)
  void SetBounds(Bounds bounds) {
    job_queue_->Schedule(Bind(&PlayerWorker::DoSetBounds, this, bounds));
  }
#endif  // SB_API_VERSION >= 4 || \
           SB_IS(PLAYER_PUNCHED_OUT)

  void SetPause(bool pause) {
    job_queue_->Schedule(Bind(&PlayerWorker::DoSetPause, this, pause));
  }

#if SB_API_VERSION >= 4
  void SetPlaybackRate(double playback_rate) {
    // Arbitrary values to give the playback rate a valid range.  A particular
    // implementation may have stricter or looser requirement, or even only
    // support several discreet values.
    // Ideally the range of the playback rate should be determined by the audio
    // graph but that will break the thread invariant of the handler.
    const double kMinimumNonZeroPlaybackRate = 0.1;
    const double kMaximumPlaybackRate = 4.0;
    if (playback_rate > 0.0 && playback_rate < kMinimumNonZeroPlaybackRate) {
      playback_rate = kMinimumNonZeroPlaybackRate;
    } else if (playback_rate > kMaximumPlaybackRate) {
      playback_rate = kMaximumPlaybackRate;
    }
    job_queue_->Schedule(
        Bind(&PlayerWorker::DoSetPlaybackRate, this, playback_rate));
  }
#endif  // SB_API_VERSION >= 4

  void UpdateDroppedVideoFrames(int dropped_video_frames) {
    host_->UpdateDroppedVideoFrames(dropped_video_frames);
  }

#if SB_API_VERSION >= 4
  SbDecodeTarget GetCurrentDecodeTarget() {
    return handler_->GetCurrentDecodeTarget();
  }
#endif  // SB_API_VERSION >= 4

 private:
  void UpdateMediaTime(SbMediaTime time);

  SbPlayerState player_state() const { return player_state_; }
  void UpdatePlayerState(SbPlayerState player_state);

  static void* ThreadEntryPoint(void* context);
  void RunLoop();
  void DoInit();
  void DoSeek(SbMediaTime seek_to_pts, int ticket);
  void DoWriteSample(InputBuffer input_buffer);
  void DoWritePendingSamples();
  void DoWriteEndOfStream(SbMediaType sample_type);
#if SB_API_VERSION >= 4 || SB_IS(PLAYER_PUNCHED_OUT)
  void DoSetBounds(Bounds bounds);
#endif  // SB_API_VERSION >= 4 || \
           SB_IS(PLAYER_PUNCHED_OUT)
  void DoSetPause(bool pause);
#if SB_API_VERSION >= 4
  void DoSetPlaybackRate(double rate);
#endif  // SB_API_VERSION >= 4
  void DoStop();

  void UpdateDecoderState(SbMediaType type, SbPlayerDecoderState state);

  SbThread thread_;
  scoped_ptr<JobQueue> job_queue_;

  Host* host_;
  scoped_ptr<Handler> handler_;

  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayer player_;
  void* context_;
  int ticket_;

  SbPlayerState player_state_;
  InputBuffer pending_audio_buffer_;
  InputBuffer pending_video_buffer_;
  Closure write_pending_sample_closure_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_
