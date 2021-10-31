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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_H_

#include <map>

#include "base/containers/small_map.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/media_session/media_metadata.h"
#include "cobalt/media_session/media_position_state.h"
#include "cobalt/media_session/media_session_action.h"
#include "cobalt/media_session/media_session_action_details.h"
#include "cobalt/media_session/media_session_playback_state.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/wrappable.h"
#include "starboard/time.h"

namespace cobalt {
namespace media_session {

class MediaSessionClient;

class MediaSession : public script::Wrappable {
  friend class MediaSessionClient;

 public:
  typedef script::CallbackFunction<void(
      const MediaSessionActionDetails& action_details)>
      MediaSessionActionHandler;
  typedef script::ScriptValue<MediaSessionActionHandler>
      MediaSessionActionHandlerHolder;
  typedef script::ScriptValue<MediaSessionActionHandler>::Reference
      MediaSessionActionHandlerReference;

 private:
  // typedef base::small_map<
  //     std::map<MediaSessionAction, MediaSessionActionHandlerReference*>,
  //     kMediaSessionActionNumActions>
  //     ActionMap;
  typedef std::map<MediaSessionAction,
                   script::ScriptValue<MediaSessionActionHandler>::Reference*>
      ActionMap;

 public:
  MediaSession();
  ~MediaSession() override;

  explicit MediaSession(MediaSessionClient* client);

  scoped_refptr<MediaMetadata> metadata() const { return metadata_; }

  void set_metadata(scoped_refptr<MediaMetadata> value);

  MediaSessionPlaybackState playback_state() const { return playback_state_; }

  void set_playback_state(MediaSessionPlaybackState state);

  void SetActionHandler(MediaSessionAction action,
                        const MediaSessionActionHandlerHolder& handler);

  void SetPositionState(base::Optional<MediaPositionState> state);

  DEFINE_WRAPPABLE_TYPE(MediaSession);
  void TraceMembers(script::Tracer* tracer) override;

  // Check whether a change task has been queued. Should only be called by
  // unit tests.
  bool IsChangeTaskQueuedForTesting() const;

  MediaSessionClient* media_session_client() {
    return media_session_client_.get();
  }

  void EnsureMediaSessionClient();

 private:
  void MaybeQueueChangeTask(base::TimeDelta delay);
  void OnChanged();

  // Returns a time representing right now - may be overridden for testing.
  virtual SbTimeMonotonic GetMonotonicNow() const {
    return SbTimeGetMonotonicNow();
  }

  ActionMap action_map_;
  std::unique_ptr<MediaSessionClient> media_session_client_;
  scoped_refptr<MediaMetadata> metadata_;
  MediaSessionPlaybackState playback_state_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  bool is_change_task_queued_;
  SbTimeMonotonic last_position_updated_time_;
  base::Optional<MediaPositionState> media_position_state_;

  DISALLOW_COPY_AND_ASSIGN(MediaSession);
};

}  // namespace media_session
}  // namespace cobalt
#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_H_
