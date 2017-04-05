// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/media_session/media_session_client.h"

namespace cobalt {
namespace media_session {

MediaSession::MediaSessionPlaybackState
MediaSessionClient::GetActualPlaybackState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Per https://wicg.github.io/mediasession/#guessed-playback-state
  // - If the "declared playback state" is "playing", then return "playing"
  // - Otherwise, return the guessed playback state
  MediaSession::MediaSessionPlaybackState declared_state;
  declared_state = media_session_->playback_state();
  if (declared_state == MediaSession::kPlaying) {
    return MediaSession::kPlaying;
  }

  if (platform_playback_state_ == MediaSession::kPlaying) {
    // "...guessed playback state is playing if any of them is
    // potentially playing and not muted..."
    return MediaSession::kPlaying;
  }

  // It's not super clear what to do when the declared state or the
  // active media session state is kPaused or kNone

  if (declared_state == MediaSession::kPaused) {
    return MediaSession::kPaused;
  }

  return MediaSession::kNone;
}

MediaSessionClient::AvailableActionsSet
MediaSessionClient::GetAvailableActions() {
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
  return AvailableActionsSet();
}

void MediaSessionClient::UpdatePlatformPlaybackState(
    MediaSession::MediaSessionPlaybackState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  MediaSession::MediaSessionPlaybackState prev_actual_state =
      GetActualPlaybackState();
  platform_playback_state_ = state;

  if (prev_actual_state != GetActualPlaybackState()) {
    OnMediaSessionChanged();
  }
}

void MediaSessionClient::InvokeAction(MediaSession::MediaSessionAction action) {
  UNREFERENCED_PARAMETER(action);
  DCHECK(thread_checker_.CalledOnValidThread());
  NOTIMPLEMENTED();
}

}  // namespace media_session
}  // namespace cobalt
