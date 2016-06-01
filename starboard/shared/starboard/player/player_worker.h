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

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/queue.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/audio_renderer_internal.h"
#include "starboard/shared/starboard/player/video_renderer_internal.h"
#include "starboard/thread.h"
#include "starboard/time.h"

// TODO: Implement DRM support
namespace starboard {
namespace shared {
namespace starboard {
namespace player {

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
    void* sample_buffer;
    int sample_buffer_size;
    SbMediaTime sample_pts;
    SbMediaVideoSampleInfo video_sample_info;
    SbDrmSampleInfo sample_drm_info;
  };

  struct WriteEndOfStreamEventData {
    SbMediaType stream_type;
  };

  struct SetPauseEventData {
    bool pause;
  };

  struct Event {
   public:
    enum Type {
      kInit,
      kSeek,
      kWriteSample,
      kWriteEndOfStream,
      kSetPause,
      kStop,
    };

    union Data {
      SeekEventData seek;
      WriteSampleEventData write_sample;
      WriteEndOfStreamEventData write_end_of_stream;
      SetPauseEventData set_pause;
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

    explicit Event(Type type) : type(type) {
      SB_DCHECK(type == kInit || type == kStop);
    }

    Type type;
    Data data;
  };

  static const SbTime kUpdateInterval = 5 * kSbTimeMillisecond;

  PlayerWorker(Host* host,
               SbMediaVideoCodec video_codec,
               SbMediaAudioCodec audio_codec,
               SbDrmSession drm,
               const SbMediaAudioHeader& audio_header,
               SbPlayerDeallocateSampleFunc sample_deallocate_func,
               SbPlayerDecoderStatusFunc decoder_status_func,
               SbPlayerStatusFunc player_status_func,
               SbPlayer player,
               void* context);
  ~PlayerWorker();

  template <typename EventData>
  void EnqueueEvent(const EventData& event_data) {
    queue_.Put(new Event(event_data));
  }

 private:
  static void* ThreadEntryPoint(void* context);
  void RunLoop();

  bool ProcessInitEvent();
  bool ProcessSeekEvent(const SeekEventData& data);
  bool ProcessWriteSampleEvent(const WriteSampleEventData& data);
  bool ProcessWriteEndOfStreamEvent(const WriteEndOfStreamEventData& data);
  bool ProcessSetPauseEvent(const SetPauseEventData& data);
  bool ProcessUpdateEvent();
  void ProcessStopEvent();

  void DeallocateSample(void* sample_buffer);
  void UpdateDecoderState(SbMediaType type);
  void UpdatePlayerState(SbPlayerState player_state);

  SbThread thread_;
  Queue<Event*> queue_;

  Host* host_;

  SbMediaVideoCodec video_codec_;
  SbMediaAudioCodec audio_codec_;
  SbMediaAudioHeader audio_header_;
  SbPlayerDeallocateSampleFunc sample_deallocate_func_;
  SbPlayerDecoderStatusFunc decoder_status_func_;
  SbPlayerStatusFunc player_status_func_;
  SbPlayer player_;
  void* context_;

  AudioRenderer* audio_renderer_;
  VideoRenderer* video_renderer_;
  SbPlayerDecoderState audio_decoder_state_;
  SbPlayerDecoderState video_decoder_state_;

  bool paused_;
  int ticket_;
  SbPlayerState player_state_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_PLAYER_WORKER_H_
