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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_H_

#include <map>

#include "cobalt/media_session/media_metadata.h"

#include "base/containers/small_map.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "cobalt/media_session/media_session_action.h"
#include "cobalt/media_session/media_session_playback_state.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"

namespace cobalt {
namespace media_session {

class MediaSessionClient;

class MediaSession : public script::Wrappable {
  friend class MediaSessionClient;

 public:
  typedef script::CallbackFunction<void()> MediaSessionActionHandler;
  typedef script::ScriptValue<MediaSessionActionHandler>
      MediaSessionActionHandlerHolder;
  typedef script::ScriptValue<MediaSessionActionHandler>::Reference
      MediaSessionActionHandlerReference;

 private:
  typedef base::SmallMap<
      std::map<MediaSessionAction, MediaSessionActionHandlerReference*>,
      kMediaSessionActionNumActions> ActionMap;

 public:
  explicit MediaSession(MediaSessionClient* client);
  ~MediaSession() override;

  scoped_refptr<MediaMetadata> metadata() const { return metadata_; }

  void set_metadata(scoped_refptr<MediaMetadata> value);

  MediaSessionPlaybackState playback_state() const { return state_; }

  void set_playback_state(MediaSessionPlaybackState state);

  void SetActionHandler(MediaSessionAction action,
                        const MediaSessionActionHandlerHolder& handler);

  DEFINE_WRAPPABLE_TYPE(MediaSession);

 private:
  void MaybeQueueChangeTask();
  void OnChanged();

  ActionMap action_map_;
  MediaSessionClient* media_session_client_;
  scoped_refptr<MediaMetadata> metadata_;
  MediaSessionPlaybackState state_;
  scoped_refptr<base::MessageLoopProxy> message_loop_;
  bool is_change_task_queued_;

  DISALLOW_COPY_AND_ASSIGN(MediaSession);
};

}  // namespace media_session
}  // namespace cobalt
#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_H_
