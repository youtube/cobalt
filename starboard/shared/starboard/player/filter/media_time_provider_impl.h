// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_

#include "starboard/common/scoped_ptr.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/filter/media_time_provider.h"
#include "starboard/shared/starboard/thread_checker.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// This class provides the media playback time when there isn't an audio track.
class MediaTimeProviderImpl : public MediaTimeProvider {
 public:
  class MonotonicSystemTimeProvider {
   public:
    virtual ~MonotonicSystemTimeProvider() {}
    virtual SbTimeMonotonic GetMonotonicNow() const = 0;
  };

  explicit MediaTimeProviderImpl(
      scoped_ptr<MonotonicSystemTimeProvider> system_time_provider);

  void SetPlaybackRate(double playback_rate) override;
  void Play() override;
  void Pause() override;
  void Seek(SbMediaTime seek_to_pts) override;
  SbMediaTime GetCurrentMediaTime(bool* is_playing,
                                  bool* is_eos_played) override;

  // When video end of stream is reached and the current media time passes the
  // video duration, |is_eos_played| of GetCurrentMediaTime() will return true.
  // When VideoEndOfStreamReached() is called without any prior calls to
  // UpdateVideoDuration(), the |seek_to_pts_| will be used as video duration.
  void UpdateVideoDuration(SbMediaTime video_duration);
  void VideoEndOfStreamReached();

 private:
  // When not NULL, |current_time| will be set to the current monotonic time
  // used to calculate the returned media time.  Note that it is possible that
  // |current_time| points to |seek_to_pts_set_at_| and the implementation
  // should handle this properly.
  SbMediaTime GetCurrentMediaTime_Locked(SbTimeMonotonic* current_time = NULL);

  ThreadChecker thread_checker_;

  scoped_ptr<MonotonicSystemTimeProvider> system_time_provider_;

  Mutex mutex_;

  double playback_rate_ = 1.0;
  bool is_playing_ = false;
  SbMediaTime seek_to_pts_ = 0;
  SbTimeMonotonic seek_to_pts_set_at_ = SbTimeGetMonotonicNow();

  SbMediaTime video_duration_ = kSbMediaTimeInvalid;
  bool is_video_end_of_stream_reached_ = false;
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_MEDIA_TIME_PROVIDER_IMPL_H_
