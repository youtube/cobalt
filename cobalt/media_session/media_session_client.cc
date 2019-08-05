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
#include <cmath>
#include <memory>

#include "starboard/time.h"

namespace cobalt {
namespace media_session {

namespace {

// Guess the media position state for the media session.
void GuessMediaPositionState(MediaSessionState* session_state,
    const media::WebMediaPlayer** guess_player,
    const media::WebMediaPlayer* current_player) {
  // Assume the player with the biggest video size is the one controlled by the
  // media session. This isn't perfect, so it's best that the web app set the
  // media position state explicitly.
  if (*guess_player == nullptr ||
      (*guess_player)->GetNaturalSize().GetArea() <
      current_player->GetNaturalSize().GetArea()) {
    *guess_player = current_player;

    MediaPositionState position_state;
    float duration = (*guess_player)->GetDuration();
    if (std::isfinite(duration)) {
      position_state.set_duration(duration);
    } else if (std::isinf(duration)) {
      position_state.set_duration(kSbTimeMax);
    } else {
      position_state.set_duration(0.0);
    }
    position_state.set_playback_rate((*guess_player)->GetPlaybackRate());
    position_state.set_position((*guess_player)->GetCurrentTime());

    *session_state = MediaSessionState(
        session_state->metadata(),
        SbTimeGetMonotonicNow(),
        position_state,
        session_state->actual_playback_state(),
        session_state->available_actions());
  }
}

}  // namespace

void MediaSessionClient::SetMediaPlayerFactory(
    const media::WebMediaPlayerFactory* factory) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  media_player_factory_ = factory;
}

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
  ComputeMediaSessionState(&session_state);
  return session_state;
}

void MediaSessionClient::ComputeMediaSessionState(
    MediaSessionState* session_state) {
  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostBlockingTask(
        FROM_HERE,
        base::Bind(&MediaSessionClient::ComputeMediaSessionState,
                   base::Unretained(this), base::Unretained(session_state)));
    return;
  }

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  *session_state = session_state_;

  // Compute the media position state if it's not set in the media session.
  if (!session_state->has_position_state() && media_player_factory_) {
    const media::WebMediaPlayer* player = nullptr;
    media_player_factory_->EnumerateWebMediaPlayers(
        base::BindRepeating(&GuessMediaPositionState,
                            session_state, &player));
  }
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

  // Compute the media position state as needed.
  MediaSessionState current_session_state;
  ComputeMediaSessionState(&current_session_state);

  OnMediaSessionStateChanged(current_session_state);
}

}  // namespace media_session
}  // namespace cobalt
