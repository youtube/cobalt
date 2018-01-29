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

#include "cobalt/media_session/media_session.h"

#include "cobalt/media_session/media_session_client.h"

namespace cobalt {
namespace media_session {

MediaSession::MediaSession(MediaSessionClient* client)
    : media_session_client_(client),
      state_(kMediaSessionPlaybackStateNone),
      message_loop_(base::MessageLoopProxy::current()),
      is_change_task_queued_(false) {}

MediaSession::~MediaSession() {
  ActionMap::iterator it;
  for (it = action_map_.begin(); it != action_map_.end(); ++it) {
    delete it->second;
  }
  action_map_.clear();
}

void MediaSession::set_metadata(scoped_refptr<MediaMetadata> value) {
  metadata_ = value;
  MaybeQueueChangeTask();
}

void MediaSession::set_playback_state(MediaSessionPlaybackState state) {
  state_ = state;
  MaybeQueueChangeTask();
}

void MediaSession::SetActionHandler(
    MediaSessionAction action, const MediaSessionActionHandlerHolder& handler) {
  // See algorithm https://wicg.github.io/mediasession/#actions-model
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_.get());
  ActionMap::iterator it = action_map_.find(action);

  if (it != action_map_.end()) {
    delete it->second;
    action_map_.erase(it);
  }
  if (!handler.IsNull()) {
    action_map_[action] = new MediaSessionActionHandlerReference(this, handler);
  }

  MaybeQueueChangeTask();
}

void MediaSession::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(metadata_);
}

void MediaSession::MaybeQueueChangeTask() {
  DCHECK_EQ(base::MessageLoopProxy::current(), message_loop_.get());
  if (is_change_task_queued_) {
    return;
  }
  is_change_task_queued_ = true;
  message_loop_->PostTask(
      FROM_HERE, base::Bind(&MediaSession::OnChanged, base::Unretained(this)));
}

void MediaSession::OnChanged() {
  is_change_task_queued_ = false;
  media_session_client_->OnMediaSessionChanged();
}

}  // namespace media_session
}  // namespace cobalt
