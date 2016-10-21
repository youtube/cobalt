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
#include "starboard/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
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

   protected:
    ~Host() {}
  };

  struct SeekEventData {
    SbMediaTime seek_to_pts;
    int ticket;
  };

  struct WriteSampleEventData {
    SbMediaType sample_type;
    InputBuffer* input_buffer;
  };

  struct WriteEndOfStreamEventData {
    SbMediaType stream_type;
  };

  struct SetPauseEventData {
    bool pause;
  };

  struct SetBoundsEventData {
    int x;
    int y;
    int width;
    int height;
  };

  struct Event {
   public:
    enum Type {
      kInit,
      kSeek,
      kWriteSample,
      kWriteEndOfStream,
      kSetPause,
      kSetBounds,
      kStop,
    };

    // TODO: No longer use a union so individual members can have non trivial
    // ctor and WriteSampleEventData::input_buffer no longer has to be a
    // pointer.
    union Data {
      SeekEventData seek;
      WriteSampleEventData write_sample;
      WriteEndOfStreamEventData write_end_of_stream;
      SetPauseEventData set_pause;
      SetBoundsEventData set_bounds;
    };

    explicit Event(const SeekEventData& seek) : type(kSeek) {
      data.seek = seek;
    }

    explicit Event(const WriteSampleEventData& write_sample)
        : type(kWriteSample) {
      data.write_sample = write_sample;
    }

    explicit Event(const WriteEndOfStreamEventData& write_end_of_stream)
        : type(kWriteEndOfStream) {
      data.write_end_of_stream = write_end_of_stream;
    }

    explicit Event(const SetPauseEventData& set_pause) : type(kSetPause) {
      data.set_pause = set_pause;
    }

    explicit Event(const SetBoundsEventData& set_bounds) : type(kSetBounds) {
      data.set_bounds = set_bounds;
    }

    explicit Event(Type type) : type(type) {
      SB_DCHECK(type == kInit || type == kStop);
    }

    Type type;
    Data data;
  };

  class Handler {
   public:
    typedef void (PlayerWorker::*UpdateMediaTimeCB)(SbMediaTime media_time);
    typedef SbPlayerState (PlayerWorker::*GetPlayerStateCB)() const;
    typedef void (PlayerWorker::*UpdatePlayerStateCB)(
        SbPlayerState player_state);

    typedef PlayerWorker::SeekEventData SeekEventData;
    typedef PlayerWorker::WriteSampleEventData WriteSampleEventData;
    typedef PlayerWorker::WriteEndOfStreamEventData WriteEndOfStreamEventData;
    typedef PlayerWorker::SetPauseEventData SetPauseEventData;
    typedef PlayerWorker::SetBoundsEventData SetBoundsEventData;

    virtual ~Handler() {}

    // This function will be called once inside PlayerWorker's ctor to setup the
    // callbacks required by the Handler.
    virtual void Setup(PlayerWorker* player_worker,
                       SbPlayer player,
                       UpdateMediaTimeCB update_media_time_cb,
                       GetPlayerStateCB get_player_state_cb,
                       UpdatePlayerStateCB update_player_state_cb) = 0;

    // All the Process* functions return false to signal a fatal error.  The
    // event processing loop in PlayerWorker will termimate in this case.
    virtual bool ProcessInitEvent() = 0;
    virtual bool ProcessSeekEvent(const SeekEventData& data) = 0;
    virtual bool ProcessWriteSampleEvent(const WriteSampleEventData& data,
                                         bool* written) = 0;
    virtual bool ProcessWriteEndOfStreamEvent(
        const WriteEndOfStreamEventData& data) = 0;
    virtual bool ProcessSetPauseEvent(const SetPauseEventData& data) = 0;
    virtual bool ProcessUpdateEvent(const SetBoundsEventData& data) = 0;
    virtual void ProcessStopEvent() = 0;
  };

  PlayerWorker(Host* host,
               scoped_ptr<Handler> handler,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayer player,
               SbTime update_interval,
               void* context);
  virtual ~PlayerWorker();

  template <typename EventData>
  void EnqueueEvent(const EventData& event_data) {
    SB_DCHECK(SbThreadIsValid(thread_));
    queue_.Put(new Event(event_data));
  }

 private:
  void UpdateMediaTime(SbMediaTime time);

  SbPlayerState player_state() const { return player_state_; }
  void UpdatePlayerState(SbPlayerState player_state);

  static void* ThreadEntryPoint(void* context);
  void RunLoop();
  bool DoInit();
  bool DoSeek(const SeekEventData& data);
  bool DoWriteSample(const WriteSampleEventData& data);
  bool DoWriteEndOfStream(const WriteEndOfStreamEventData& data);
  bool DoUpdate(const SetBoundsEventData& data);
  void DoStop();

  void UpdateDecoderState(SbMediaType type, SbPlayerDecoderState state);

  SbThread thread_;
  Queue<Event*> queue_;

  Host* host_;
  scoped_ptr<Handler> handler_;

  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayer player_;
  void* context_;
  int ticket_;

  SbPlayerState player_state_;
  const SbTime update_interval_;
  WriteSampleEventData pending_audio_sample_;
  WriteSampleEventData pending_video_sample_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_
