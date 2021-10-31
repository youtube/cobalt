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

#include "cobalt/media_session/media_session.h"

#include "cobalt/media_session/media_session_client.h"

namespace cobalt {
namespace media_session {

MediaSession::MediaSession()
    : playback_state_(kMediaSessionPlaybackStateNone),
      task_runner_(base::MessageLoop::current()->task_runner()),
      is_change_task_queued_(false),
      last_position_updated_time_(0) {}

MediaSession::~MediaSession() {
  ActionMap::iterator it;
  for (it = action_map_.begin(); it != action_map_.end(); ++it) {
    delete it->second;
  }
  action_map_.clear();
}

MediaSession::MediaSession(MediaSessionClient* client)
    : media_session_client_(client),
      playback_state_(kMediaSessionPlaybackStateNone),
      task_runner_(base::MessageLoop::current()->task_runner()),
      is_change_task_queued_(false),
      last_position_updated_time_(0) {}

void MediaSession::set_metadata(scoped_refptr<MediaMetadata> value) {
  metadata_ = value;
  MaybeQueueChangeTask(base::TimeDelta());
}

void MediaSession::set_playback_state(
    MediaSessionPlaybackState playback_state) {
  playback_state_ = playback_state;
  MaybeQueueChangeTask(base::TimeDelta());
}

void MediaSession::SetActionHandler(
    MediaSessionAction action, const MediaSessionActionHandlerHolder& handler) {
  // See algorithm https://wicg.github.io/mediasession/#actions-model
  DCHECK(task_runner_->BelongsToCurrentThread());
  ActionMap::iterator it = action_map_.find(action);

  if (it != action_map_.end()) {
    delete it->second;
    action_map_.erase(it);
  }
  if (!handler.IsNull()) {
    action_map_[action] = new MediaSessionActionHandlerReference(this, handler);
  }

  MaybeQueueChangeTask(base::TimeDelta());
}

void MediaSession::SetPositionState(base::Optional<MediaPositionState> state) {
  last_position_updated_time_ = GetMonotonicNow();
  media_position_state_ = state;
  MaybeQueueChangeTask(base::TimeDelta());
}

void MediaSession::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(metadata_.get());
}

bool MediaSession::IsChangeTaskQueuedForTesting() const {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return is_change_task_queued_;
}

void MediaSession::EnsureMediaSessionClient() {
  if (media_session_client_ == nullptr) {
    media_session_client_ = std::make_unique<MediaSessionClient>();
    DCHECK(media_session_client_);
    media_session_client_->set_media_session(this);
  }
}

void MediaSession::MaybeQueueChangeTask(base::TimeDelta delay) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (is_change_task_queued_) {
    return;
  }
  is_change_task_queued_ = true;
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&MediaSession::OnChanged, this),
      delay);
}

void MediaSession::OnChanged() {
  is_change_task_queued_ = false;
  if (media_session_client_) {
    media_session_client_->UpdateMediaSessionState();
  }
}

}  // namespace media_session
}  // namespace cobalt
