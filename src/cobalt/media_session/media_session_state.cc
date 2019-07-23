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

#include "cobalt/media_session/media_session_state.h"

#include <algorithm>

namespace cobalt {
namespace media_session {

MediaSessionState::MediaSessionState(
    base::Optional<MediaMetadataInit> metadata,
    SbTimeMonotonic last_position_updated_time,
    const base::Optional<MediaPositionState>& media_position_state,
    MediaSessionPlaybackState actual_playback_state,
    AvailableActionsSet available_actions)
    : metadata_(metadata),
      last_position_updated_time_(last_position_updated_time),
      actual_playback_state_(actual_playback_state),
      available_actions_(available_actions) {
  if (media_position_state) {
    double duration = media_position_state->duration();
    duration_ = (duration >= static_cast<double>(kSbTimeMax))
                    ? kSbTimeMax
                    : static_cast<SbTimeMonotonic>(duration * kSbTimeSecond);

    actual_playback_rate_ =
        (actual_playback_state_ == kMediaSessionPlaybackStatePaused)
            ? 0.0
            : media_position_state->playback_rate();

    last_position_ = static_cast<SbTimeMonotonic>(
        media_position_state->position() * kSbTimeSecond);
  }
}

SbTimeMonotonic MediaSessionState::GetCurrentPlaybackPosition(
    SbTimeMonotonic monotonic_now) const {
  SbTimeMonotonic time_elapsed = monotonic_now - last_position_updated_time_;
  time_elapsed = static_cast<SbTimeMonotonic>(
      static_cast<double>(time_elapsed) * actual_playback_rate_);
  SbTimeMonotonic position = time_elapsed + last_position_;
  return std::max(static_cast<SbTimeMonotonic>(0),
                  std::min(duration(), position));
}

bool MediaSessionState::operator==(const MediaSessionState& other) const {
  return other.last_position_updated_time_ == last_position_updated_time_ &&
         other.actual_playback_state_ == actual_playback_state_ &&
         other.available_actions_ == available_actions_ &&
         other.duration_ == duration_ &&
         other.actual_playback_rate_ == actual_playback_rate_ &&
         other.last_position_ == last_position_ && other.metadata_ == metadata_;
}
bool MediaSessionState::operator!=(const MediaSessionState& other) const {
  return !(*this == other);
}

bool operator==(const MediaMetadataInit& a, const MediaMetadataInit& b) {
  return a.title() == b.title() && a.artist() == b.artist() &&
         a.album() == b.album() && a.has_artwork() == b.has_artwork() &&
         (!a.has_artwork() || a.artwork() == b.artwork());
}
bool operator!=(const MediaMetadataInit& a, const MediaMetadataInit& b) {
  return !(a == b);
}
bool operator==(const script::Sequence<MediaImage>& a,
                const script::Sequence<MediaImage>& b) {
  if (a.size() != b.size()) return false;
  for (script::Sequence<MediaImage>::size_type i = 0, max = a.size(); i < max;
       i++) {
    if (a.at(i) != b.at(i)) return false;
  }
  return true;
}
bool operator!=(const script::Sequence<MediaImage>& a,
                const script::Sequence<MediaImage>& b) {
  return !(a == b);
}
bool operator==(const MediaImage& a, const MediaImage& b) {
  return a.src() == b.src() && a.sizes() == b.sizes() && a.type() == b.type();
}
bool operator!=(const MediaImage& a, const MediaImage& b) { return !(a == b); }

}  // namespace media_session
}  // namespace cobalt
