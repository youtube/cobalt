// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_STATE_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_STATE_H_

#include <bitset>

#include "base/optional.h"
#include "cobalt/media_session/media_metadata_init.h"
#include "cobalt/media_session/media_position_state.h"
#include "cobalt/media_session/media_session_action.h"
#include "cobalt/media_session/media_session_playback_state.h"
#include "starboard/time.h"

namespace cobalt {
namespace media_session {

// Immutable state describing the current playback.
class MediaSessionState {
 public:
  typedef std::bitset<kMediaSessionActionNumActions> AvailableActionsSet;

  MediaSessionState() {}

  MediaSessionState(
      base::Optional<MediaMetadataInit> metadata,
      SbTimeMonotonic last_position_updated_time,
      const base::Optional<MediaPositionState>& media_position_state,
      MediaSessionPlaybackState actual_playback_state,
      AvailableActionsSet available_actions);

  // Explicitly copyable by design.
  MediaSessionState(const MediaSessionState& other) = default;
  MediaSessionState& operator=(const MediaSessionState& other) = default;

  // Returns true if the metadata is set.
  bool has_metadata() const { return metadata_.has_value(); }

  // Returns the metadata about the currently playing media.
  const base::Optional<MediaMetadataInit>& metadata() const {
    return metadata_;
  }

  // Returns whether media position state was specified.
  bool has_position_state() const { return last_position_updated_time_ != 0; }

  // Returns the position of the current playback.
  // https://wicg.github.io/mediasession/#current-playback-position
  // Returns the position
  SbTimeMonotonic current_playback_position() const {
    return GetCurrentPlaybackPosition(SbTimeGetMonotonicNow());
  }

  // Returns the position of the current playback, given the current time.
  // This may be used for testing without calling |SbTimeGetMonotonicNow|.
  SbTimeMonotonic GetCurrentPlaybackPosition(
      SbTimeMonotonic monotonic_now) const;

  // Returns a coefficient of the current playback rate. e.g. 1.0 is normal
  // forward playback, negative for reverse playback, and 0.0 when paused.
  // https://wicg.github.io/mediasession/#actual-playback-rate
  double actual_playback_rate() const { return actual_playback_rate_; }

  // Returns the duration of the currently playing media. 0 if no media is
  // playing or the web app has not reported the position state. kSbTimeMax if
  // there is no defined duration such as live playback.
  // https://wicg.github.io/mediasession/#dom-mediapositionstate-duration
  SbTimeMonotonic duration() const { return duration_; }

  // Returns the actual playback state.
  // https://wicg.github.io/mediasession/#actual-playback-state
  MediaSessionPlaybackState actual_playback_state() const {
    return actual_playback_state_;
  }

  // Returns the set of currently available mediasession actions
  // per "media session actions update algorithm"
  // https://wicg.github.io/mediasession/#actions-model
  AvailableActionsSet available_actions() const { return available_actions_; }

  bool operator==(const MediaSessionState& other) const;
  bool operator!=(const MediaSessionState& other) const;

 private:
  base::Optional<MediaMetadataInit> metadata_;
  SbTimeMonotonic last_position_updated_time_ = 0;
  SbTimeMonotonic last_position_ = 0;
  double actual_playback_rate_ = 0.0;
  SbTimeMonotonic duration_ = 0;
  MediaSessionPlaybackState actual_playback_state_ =
      kMediaSessionPlaybackStateNone;
  AvailableActionsSet available_actions_;
};

// Comparison operators for metadata.
bool operator==(const MediaMetadataInit& a, const MediaMetadataInit& b);
bool operator!=(const MediaMetadataInit& a, const MediaMetadataInit& b);
bool operator==(const script::Sequence<MediaImage>& a,
                const script::Sequence<MediaImage>& b);
bool operator!=(const script::Sequence<MediaImage>& a,
                const script::Sequence<MediaImage>& b);
bool operator==(const MediaImage& a, const MediaImage& b);
bool operator!=(const MediaImage& a, const MediaImage& b);

}  // namespace media_session
}  // namespace cobalt

#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_STATE_H_
