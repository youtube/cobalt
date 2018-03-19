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

#include <functional>
#include <string>

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
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
    virtual void UpdateMediaTime(SbTime media_time, int ticket) = 0;
    virtual void UpdateDroppedVideoFrames(int dropped_video_frames) = 0;

   protected:
    virtual ~Host() {}
  };

  struct Bounds {
    int z_index;
    int x;
    int y;
    int width;
    int height;
  };

  // All functions of this class will be called from the JobQueue thread.
  class Handler {
   public:
    typedef void (PlayerWorker::*UpdateMediaTimeCB)(SbTime media_time);
    typedef SbPlayerState (PlayerWorker::*GetPlayerStateCB)() const;
    typedef void (PlayerWorker::*UpdatePlayerStateCB)(
        SbPlayerState player_state);
    typedef void (PlayerWorker::*UpdatePlayerErrorCB)(
        const std::string& message);
    virtual ~Handler() {}

    // All the following functions return false to signal a fatal error.  The
    // event processing loop in PlayerWorker will termimate in this case.
    virtual bool Init(PlayerWorker* player_worker,
                      JobQueue* job_queue,
                      SbPlayer player,
                      UpdateMediaTimeCB update_media_time_cb,
                      GetPlayerStateCB get_player_state_cb,
                      UpdatePlayerStateCB update_player_state_cb
#if SB_HAS(PLAYER_ERROR_MESSAGE)
                      ,
                      UpdatePlayerErrorCB update_player_error_cb
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
                      ) = 0;
    virtual bool Seek(SbTime seek_to_time, int ticket) = 0;
    virtual bool WriteSample(const scoped_refptr<InputBuffer>& input_buffer,
                             bool* written) = 0;
    virtual bool WriteEndOfStream(SbMediaType sample_type) = 0;
    virtual bool SetPause(bool pause) = 0;
    virtual bool SetPlaybackRate(double playback_rate) = 0;
    virtual void SetVolume(double volume) = 0;

    virtual bool SetBounds(const Bounds& bounds) = 0;

    // Once this function returns, all processing on the Handler and related
    // objects has to be stopped.  The JobQueue will be destroyed immediately
    // after and is no longer safe to access.
    virtual void Stop() = 0;

    virtual SbDecodeTarget GetCurrentDecodeTarget() = 0;
  };

  PlayerWorker(Host* host,
               SbMediaAudioCodec audio_codec,
               scoped_ptr<Handler> handler,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
#if SB_HAS(PLAYER_ERROR_MESSAGE)
               SbPlayerErrorFunc player_error_func,
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
               SbPlayer player,
               void* context);
  ~PlayerWorker();

  void Seek(SbTime seek_to_time, int ticket) {
    job_queue_->Schedule(
        std::bind(&PlayerWorker::DoSeek, this, seek_to_time, ticket));
  }

  void WriteSample(const scoped_refptr<InputBuffer>& input_buffer) {
    job_queue_->Schedule(
        std::bind(&PlayerWorker::DoWriteSample, this, input_buffer));
  }

  void WriteEndOfStream(SbMediaType sample_type) {
    job_queue_->Schedule(
        std::bind(&PlayerWorker::DoWriteEndOfStream, this, sample_type));
  }

  void SetBounds(Bounds bounds) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoSetBounds, this, bounds));
  }

  void SetPause(bool pause) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoSetPause, this, pause));
  }

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
        std::bind(&PlayerWorker::DoSetPlaybackRate, this, playback_rate));
  }

  void SetVolume(double volume) {
    job_queue_->Schedule(std::bind(&PlayerWorker::DoSetVolume, this, volume));
  }

  void UpdateDroppedVideoFrames(int dropped_video_frames) {
    host_->UpdateDroppedVideoFrames(dropped_video_frames);
  }

  SbDecodeTarget GetCurrentDecodeTarget() {
    return handler_->GetCurrentDecodeTarget();
  }

 private:
  void UpdateMediaTime(SbTime time);

  SbPlayerState player_state() const { return player_state_; }
  void UpdatePlayerState(SbPlayerState player_state);
  void UpdatePlayerError(const std::string& message);

  static void* ThreadEntryPoint(void* context);
  void RunLoop();
  void DoInit();
  void DoSeek(SbTime seek_to_time, int ticket);
  void DoWriteSample(const scoped_refptr<InputBuffer>& input_buffer);
  void DoWritePendingSamples();
  void DoWriteEndOfStream(SbMediaType sample_type);
  void DoSetBounds(Bounds bounds);
  void DoSetPause(bool pause);
  void DoSetPlaybackRate(double rate);
  void DoSetVolume(double volume);
  void DoStop();

  void UpdateDecoderState(SbMediaType type, SbPlayerDecoderState state);

  SbThread thread_;
  scoped_ptr<JobQueue> job_queue_;

  Host* host_;
  SbMediaAudioCodec audio_codec_;
  scoped_ptr<Handler> handler_;

  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
#if SB_HAS(PLAYER_ERROR_MESSAGE)
  SbPlayerErrorFunc player_error_func_;
#endif  // SB_HAS(PLAYER_ERROR_MESSAGE)
  bool error_occurred_ = false;
  SbPlayer player_;
  void* context_;
  int ticket_;

  SbPlayerState player_state_;
  scoped_refptr<InputBuffer> pending_audio_buffer_;
  scoped_refptr<InputBuffer> pending_video_buffer_;
  JobQueue::JobToken write_pending_sample_job_token_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_
