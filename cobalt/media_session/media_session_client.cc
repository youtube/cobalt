// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <algorithm>
#include <memory>

namespace cobalt {
namespace media_session {

MediaSessionPlaybackState MediaSessionClient::ComputeActualPlaybackState()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

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

MediaSessionState::AvailableActionsSet
MediaSessionClient::ComputeAvailableActions() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // "Available actions" are determined based on active media session
  // and supported media session actions.
  // Note for cobalt, there's only one window/tab so there's only one
  // "active media session"
  // https://wicg.github.io/mediasession/#actions-model
  //
  // Note that this is essentially the "media session actions update algorithm"
  // inverted.
  MediaSessionState::AvailableActionsSet result =
      MediaSessionState::AvailableActionsSet();

  for (MediaSession::ActionMap::iterator it =
           media_session_->action_map_.begin();
       it != media_session_->action_map_.end(); ++it) {
    result[it->first] = true;
  }

  switch (ComputeActualPlaybackState()) {
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
  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::UpdatePlatformPlaybackState,
                              base::Unretained(this), state));
    return;
  }

  platform_playback_state_ = state;
  if (session_state_.actual_playback_state() != ComputeActualPlaybackState()) {
    UpdateMediaSessionState();
  }
}

void MediaSessionClient::InvokeActionInternal(
    std::unique_ptr<MediaSessionActionDetails> details) {
  DCHECK(details->has_action());

  // Some fields should only be set for applicable actions.
  DCHECK(!details->has_seek_offset() ||
         details->action() == kMediaSessionActionSeekforward ||
         details->action() == kMediaSessionActionSeekbackward);
  DCHECK(!details->has_seek_time() ||
         details->action() == kMediaSessionActionSeekto);
  DCHECK(!details->has_fast_seek() ||
         details->action() == kMediaSessionActionSeekto);

  // Seek times/offsets are non-negative, even for seeking backwards.
  DCHECK(!details->has_seek_time() || details->seek_time() >= 0.0);
  DCHECK(!details->has_seek_offset() || details->seek_offset() >= 0.0);

  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::InvokeActionInternal,
                              base::Unretained(this), base::Passed(&details)));
    return;
  }

  MediaSession::ActionMap::iterator it =
      media_session_->action_map_.find(details->action());

  if (it == media_session_->action_map_.end()) {
    return;
  }

  it->second->value().Run(*details);
}

MediaSessionState MediaSessionClient::GetMediaSessionState() {
  MediaSessionState session_state;
  GetMediaSessionStateInternal(&session_state);
  return session_state;
}

void MediaSessionClient::GetMediaSessionStateInternal(
    MediaSessionState* session_state) {
  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&MediaSessionClient::GetMediaSessionStateInternal,
                   base::Unretained(this), base::Unretained(session_state)));
    return;
  }

  *session_state = session_state_;
}

void MediaSessionClient::UpdateMediaSessionState() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<MediaMetadata> session_metadata(media_session_->metadata());
  base::Optional<MediaMetadataInit> metadata;
  if (session_metadata) {
    metadata.emplace();
    metadata->set_title(session_metadata->title());
    metadata->set_artist(session_metadata->artist());
    metadata->set_album(session_metadata->album());
    metadata->set_artwork(session_metadata->artwork());
  }
  session_state_ = MediaSessionState(
      metadata,
      media_session_->last_position_updated_time_,
      media_session_->media_position_state_,
      ComputeActualPlaybackState(),
      ComputeAvailableActions());
  OnMediaSessionStateChanged(session_state_);
}

}  // namespace media_session
}  // namespace cobalt
