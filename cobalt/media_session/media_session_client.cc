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
#include <string>

#include "base/logging.h"
#include "cobalt/media_session/media_image.h"
#include "cobalt/script/sequence.h"
#include "starboard/time.h"

namespace cobalt {
namespace media_session {

using MediaImageSequence = ::cobalt::script::Sequence<MediaImage>;

namespace {

// Delay to re-query position state after an action has been invoked.
const base::TimeDelta kUpdateDelay = base::TimeDelta::FromMilliseconds(250);

// Delay to check if the media session state is not active.
const base::TimeDelta kMaybeFreezeDelay =
    base::TimeDelta::FromMilliseconds(1500);

int64_t GetPlaybackArea(const media::WebMediaPlayer* player) {
  int width = player->GetNaturalWidth();
  int height = player->GetNaturalHeight();
  DCHECK_GE(width, 0);
  DCHECK_GE(height, 0);
  return static_cast<int64_t>(width) * height;
}

// Guess the media position state for the media session.
void GuessMediaPositionState(MediaSessionState* session_state,
                             const media::WebMediaPlayer** guess_player,
                             const media::WebMediaPlayer* current_player) {
  // Assume the player with the biggest video size is the one controlled by the
  // media session. This isn't perfect, so it's best that the web app set the
  // media position state explicitly.
  if (*guess_player == nullptr ||
      GetPlaybackArea(*guess_player) < GetPlaybackArea(current_player)) {
    *guess_player = current_player;

    MediaPositionState position_state;
    const double duration = (*guess_player)->GetDuration();
    if (std::isfinite(duration)) {
      position_state.set_duration(duration);
    } else if (std::isinf(duration)) {
      position_state.set_duration(kSbTimeMax);
    } else {
      position_state.set_duration(0.0);
    }
    position_state.set_playback_rate((*guess_player)->GetPlaybackRate());
    position_state.set_position((*guess_player)->GetCurrentTime());

    *session_state = MediaSessionState(session_state->metadata(),
                                       SbTimeGetMonotonicNow(), position_state,
                                       session_state->actual_playback_state(),
                                       session_state->available_actions());
  }
}
}  // namespace

MediaSessionClient::MediaSessionClient(MediaSession* media_session)
    : media_session_(media_session),
      platform_playback_state_(kMediaSessionPlaybackStateNone),
      sequence_number_(0) {
  extension_ = static_cast<const CobaltExtensionMediaSessionApi*>(
      SbSystemGetExtension(kCobaltExtensionMediaSessionName));
  if (extension_) {
    if (strcmp(extension_->name, kCobaltExtensionMediaSessionName) != 0 ||
        extension_->version < 1) {
      LOG(WARNING) << "Wrong MediaSession extension supplied";
      extension_ = nullptr;
    } else if (extension_->RegisterMediaSessionCallbacks != nullptr) {
      extension_->RegisterMediaSessionCallbacks(
          this, &InvokeActionCallback, &UpdatePlatformPlaybackStateCallback);
      DCHECK(extension_->DestroyMediaSessionClientCallback)
          << "Possible heap use-after-free if platform does not handle media "
          << "session DestroyMediaSessionClientCallback()";
    }
  }
}

MediaSessionClient::~MediaSessionClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Destroy the platform's MediaSessionClient, if it exists.
  if (extension_ != NULL &&
      extension_->DestroyMediaSessionClientCallback != NULL) {
    extension_->DestroyMediaSessionClientCallback();
  }
}

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
      // Not defined in the spec: disable Seekbackward, Seekforward, SeekTo, &
      // Stop when no media is playing.
      result[kMediaSessionActionSeekbackward] = false;
      result[kMediaSessionActionSeekforward] = false;
      result[kMediaSessionActionSeekto] = false;
      result[kMediaSessionActionStop] = false;
    // Fall-through intended (None case falls through to Paused case).
    case kMediaSessionPlaybackStatePaused:
      // "Otherwise, remove pause from available actions."
      result[kMediaSessionActionPause] = false;
      break;
  }

  return result;
}

void MediaSessionClient::PostDelayedTaskForMaybeFreezeCallback() {
  media_session_->task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaSessionClient::RunMaybeFreezeCallback, AsWeakPtr(),
                 ++sequence_number_),
      kMaybeFreezeDelay);
}

void MediaSessionClient::UpdatePlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::UpdatePlatformPlaybackState,
                              AsWeakPtr(), state));
    return;
  }

  platform_playback_state_ = ConvertPlaybackState(state);
  if (session_state_.actual_playback_state() != ComputeActualPlaybackState()) {
    UpdateMediaSessionState();
  }

  PostDelayedTaskForMaybeFreezeCallback();
}

void MediaSessionClient::RunMaybeFreezeCallback(int sequence_number) {
  if (sequence_number != sequence_number_) return;

  if (!is_active() && !maybe_freeze_callback_.is_null()) {
    maybe_freeze_callback_.Run();
  }
}

void MediaSessionClient::InvokeActionInternal(
    std::unique_ptr<CobaltExtensionMediaSessionActionDetails> details) {
  DCHECK(details->action >= 0 &&
         details->action < kCobaltExtensionMediaSessionActionNumActions);

  // Some fields should only be set for applicable actions.
  DCHECK(details->seek_offset < 0.0 ||
         details->action == kCobaltExtensionMediaSessionActionSeekforward ||
         details->action == kCobaltExtensionMediaSessionActionSeekbackward);
  DCHECK(details->seek_time < 0.0 ||
         details->action == kCobaltExtensionMediaSessionActionSeekto);
  DCHECK(!details->fast_seek ||
         details->action == kCobaltExtensionMediaSessionActionSeekto);

  DCHECK(media_session_->task_runner_);
  if (!media_session_->task_runner_->BelongsToCurrentThread()) {
    media_session_->task_runner_->PostTask(
        FROM_HERE, base::Bind(&MediaSessionClient::InvokeActionInternal,
                              AsWeakPtr(), base::Passed(&details)));
    return;
  }

  MediaSession::ActionMap::iterator it = media_session_->action_map_.find(
      ConvertMediaSessionAction(details->action));

  if (it == media_session_->action_map_.end()) {
    return;
  }

  std::unique_ptr<MediaSessionActionDetails> script_details =
      ConvertActionDetails(*details);
  it->second->value().Run(*script_details);

  // Queue a session update to reflect the effects of the action.
  if (!media_session_->media_position_state_) {
    media_session_->MaybeQueueChangeTask(kUpdateDelay);
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
      metadata, media_session_->last_position_updated_time_,
      media_session_->media_position_state_, ComputeActualPlaybackState(),
      ComputeAvailableActions());

  // Compute the media position state if it's not set in the media session.
  if (!media_session_->media_position_state_ && media_player_factory_) {
    const media::WebMediaPlayer* player = nullptr;
    media_player_factory_->EnumerateWebMediaPlayers(base::BindRepeating(
        &GuessMediaPositionState, &session_state_, &player));

    // The media duration may be reported as 0 when seeking. Re-query the
    // media session state after a delay.
    if (session_state_.actual_playback_state() ==
            kMediaSessionPlaybackStatePlaying &&
        session_state_.duration() == 0) {
      media_session_->MaybeQueueChangeTask(kUpdateDelay);
    }
  }

  OnMediaSessionStateChanged(session_state_);
}

void MediaSessionClient::OnMediaSessionStateChanged(
    const MediaSessionState& session_state) {
  if (extension_ && extension_->version >= 1) {
    CobaltExtensionMediaSessionState ext_state;
    size_t artwork_size = 0;
    if (session_state.has_metadata() &&
        session_state.metadata().value().has_artwork()) {
      artwork_size = session_state.metadata().value().artwork().size();
    }
    std::unique_ptr<CobaltExtensionMediaImage[]> ext_artwork =
        std::unique_ptr<CobaltExtensionMediaImage[]>(
            new CobaltExtensionMediaImage[artwork_size]);

    ext_state.duration = session_state.duration();
    ext_state.actual_playback_rate = session_state.actual_playback_rate();
    ext_state.current_playback_position =
        session_state.current_playback_position();
    ext_state.has_position_state = session_state.has_position_state();
    ext_state.actual_playback_state =
        ConvertPlaybackState(session_state.actual_playback_state());
    ConvertMediaSessionActions(session_state.available_actions(),
                               ext_state.available_actions);
    std::string album = "";
    std::string artist = "";
    std::string title = "";

    if (session_state.has_metadata()) {
      const MediaMetadataInit& metadata = session_state.metadata().value();
      album = metadata.album();
      artist = metadata.artist();
      title = metadata.title();
      if (artwork_size > 0) {
        const MediaImageSequence& artwork(metadata.artwork());
        for (MediaImageSequence::size_type i = 0; i < artwork_size; i++) {
          const MediaImage& media_image(artwork.at(i));
          CobaltExtensionMediaImage ext_image;
          ext_image.src = media_image.src().c_str();
          if (ext_image.src == nullptr) {
            // src() is required, but Cobalt IDL parser doesn't enforce it.
            // See cobalt/media_session/media_image.idl for more info.
            // https://wicg.github.io/mediasession/#dictdef-mediaimage
            LOG(ERROR) << "Required src string for MediaImage is missing.";
          }
          ext_image.size = media_image.sizes().c_str();
          ext_image.type = media_image.type().c_str();
          ext_artwork[i] = ext_image;
        }
      }
    }
    CobaltExtensionMediaMetadata ext_metadata = {
        album.c_str(), artist.c_str(), title.c_str(), ext_artwork.get(),
        artwork_size};
    ext_state.metadata = &ext_metadata;

    extension_->OnMediaSessionStateChanged(ext_state);
  }
}

// static
void MediaSessionClient::UpdatePlatformPlaybackStateCallback(
    CobaltExtensionMediaSessionPlaybackState state, void* callback_context) {
  MediaSessionClient* client =
      static_cast<MediaSessionClient*>(callback_context);
  client->UpdatePlatformPlaybackState(state);
}

// static
void MediaSessionClient::InvokeActionCallback(
    CobaltExtensionMediaSessionActionDetails details, void* callback_context) {
  MediaSessionClient* client =
      static_cast<MediaSessionClient*>(callback_context);
  client->InvokeAction(details);
}

CobaltExtensionMediaSessionPlaybackState
MediaSessionClient::ConvertPlaybackState(MediaSessionPlaybackState state) {
  switch (state) {
    case kMediaSessionPlaybackStatePlaying:
      return kCobaltExtensionMediaSessionPlaying;
    case kMediaSessionPlaybackStatePaused:
      return kCobaltExtensionMediaSessionPaused;
    case kMediaSessionPlaybackStateNone:
    default:
      return kCobaltExtensionMediaSessionNone;
  }
}

MediaSessionPlaybackState MediaSessionClient::ConvertPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  switch (state) {
    case kCobaltExtensionMediaSessionPlaying:
      return kMediaSessionPlaybackStatePlaying;
    case kCobaltExtensionMediaSessionPaused:
      return kMediaSessionPlaybackStatePaused;
    case kCobaltExtensionMediaSessionNone:
    default:
      return kMediaSessionPlaybackStateNone;
  }
}

void MediaSessionClient::ConvertMediaSessionActions(
    const MediaSessionState::AvailableActionsSet& actions,
    bool result[kCobaltExtensionMediaSessionActionNumActions]) {
  for (int i = 0; i < kCobaltExtensionMediaSessionActionNumActions; i++) {
    result[i] = false;
    MediaSessionAction action = static_cast<MediaSessionAction>(i);
    if (actions[action]) {
      result[ConvertMediaSessionAction(action)] = true;
    }
  }
}

std::unique_ptr<MediaSessionActionDetails>
MediaSessionClient::ConvertActionDetails(
    const CobaltExtensionMediaSessionActionDetails& ext_details) {
  std::unique_ptr<MediaSessionActionDetails> details(
      new MediaSessionActionDetails());
  details->set_action(ConvertMediaSessionAction(ext_details.action));
  if (ext_details.seek_offset >= 0.0) {
    details->set_seek_offset(ext_details.seek_offset);
  }
  if (ext_details.seek_time >= 0.0) {
    details->set_seek_time(ext_details.seek_time);
  }
  details->set_fast_seek(ext_details.fast_seek);
  return details;
}

CobaltExtensionMediaSessionAction MediaSessionClient::ConvertMediaSessionAction(
    MediaSessionAction action) {
  switch (action) {
    case kMediaSessionActionPause:
      return kCobaltExtensionMediaSessionActionPause;
    case kMediaSessionActionSeekbackward:
      return kCobaltExtensionMediaSessionActionSeekbackward;
    case kMediaSessionActionPrevioustrack:
      return kCobaltExtensionMediaSessionActionPrevioustrack;
    case kMediaSessionActionNexttrack:
      return kCobaltExtensionMediaSessionActionNexttrack;
    case kMediaSessionActionSeekforward:
      return kCobaltExtensionMediaSessionActionSeekforward;
    case kMediaSessionActionSeekto:
      return kCobaltExtensionMediaSessionActionSeekto;
    case kMediaSessionActionStop:
      return kCobaltExtensionMediaSessionActionStop;
    case kMediaSessionActionPlay:
    default:
      return kCobaltExtensionMediaSessionActionPlay;
  }
}

MediaSessionAction MediaSessionClient::ConvertMediaSessionAction(
    CobaltExtensionMediaSessionAction action) {
  switch (action) {
    case kCobaltExtensionMediaSessionActionPause:
      return kMediaSessionActionPause;
    case kCobaltExtensionMediaSessionActionSeekbackward:
      return kMediaSessionActionSeekbackward;
    case kCobaltExtensionMediaSessionActionPrevioustrack:
      return kMediaSessionActionPrevioustrack;
    case kCobaltExtensionMediaSessionActionNexttrack:
      return kMediaSessionActionNexttrack;
    case kCobaltExtensionMediaSessionActionSeekforward:
      return kMediaSessionActionSeekforward;
    case kCobaltExtensionMediaSessionActionSeekto:
      return kMediaSessionActionSeekto;
    case kCobaltExtensionMediaSessionActionStop:
      return kMediaSessionActionStop;
    case kCobaltExtensionMediaSessionActionPlay:
    default:
      return kMediaSessionActionPlay;
  }
}
}  // namespace media_session
}  // namespace cobalt
