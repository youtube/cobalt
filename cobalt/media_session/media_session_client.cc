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

MediaSessionPlaybackState MediaSessionClient::GetActualPlaybackState() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Per https://wicg.github.io/mediasession/#guessed-playback-state
  // - If the "declared playback state" is "playing", then return "playing"
  // - Otherwise, return the guessed playback state
  MediaSessionPlaybackState declared_state;
  declared_state = media_session_->playback_state();
  if (declared_state == kMediaSessionPlaybackStatePlaying) {
    return kMediaSessionPlaybackStatePlaying;
  }

  if (platform_playback_state_ == kMediaSessionPlaybackStatePlaying) {
    // "...guessed playback state is playing if any of them is
    // potentially playing and not muted..."
    return kMediaSessionPlaybackStatePlaying;
  }

  // It's not super clear what to do when the declared state or the
  // active media session state is kPaused or kNone

  if (declared_state == kMediaSessionPlaybackStatePaused) {
    return kMediaSessionPlaybackStatePaused;
  }

  return kMediaSessionPlaybackStateNone;
}

MediaSessionClient::AvailableActionsSet
MediaSessionClient::GetAvailableActions() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // "Available actions" are determined based on active media session
  // and supported media session actions.
  // Note for cobalt, there's only one window/tab so there's only one
  // "active media session"
  // https://wicg.github.io/mediasession/#actions-model
  //
  // Note that this is essentially the "media session actions update algorithm"
  // inverted.
  AvailableActionsSet result = AvailableActionsSet();

  for (MediaSession::ActionMap::iterator it =
         media_session_->action_map_.begin();
       it != media_session_->action_map_.end();
       ++it) {
    result[it->first] = true;
  }

  switch (GetActualPlaybackState()) {
    case kMediaSessionPlaybackStatePlaying:
      // "If the active media session’s actual playback state is playing, remove
      // play from available actions."
      result[kMediaSessionActionPlay] = false;
      break;
    case kMediaSessionPlaybackStateNone:
    case kMediaSessionPlaybackStatePaused:
      // "Otherwise, remove pause from available actions."
      result[kMediaSessionActionPause] = false;
      break;
  }

  return result;
}

void MediaSessionClient::UpdatePlatformPlaybackState(
    MediaSessionPlaybackState state) {
  if (base::MessageLoopProxy::current() != media_session_->message_loop_) {
    media_session_->message_loop_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::UpdatePlatformPlaybackState,
                              base::Unretained(this), state));
    return;
  }

  MediaSessionPlaybackState prev_actual_state = GetActualPlaybackState();
  platform_playback_state_ = state;

  if (prev_actual_state != GetActualPlaybackState()) {
    OnMediaSessionChanged();
  }
}

void MediaSessionClient::InvokeAction(MediaSessionAction action) {
  if (base::MessageLoopProxy::current() != media_session_->message_loop_) {
    media_session_->message_loop_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::InvokeAction,
                              base::Unretained(this), action));
    return;
  }

  MediaSession::ActionMap::iterator it =
      media_session_->action_map_.find(action);

  if (it == media_session_->action_map_.end()) {
    return;
  }

  it->second->value().Run();
}

}  // namespace media_session
}  // namespace cobalt
