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

#include "cobalt/media_session/media_metadata.h"

#include "base/logging.h"
#include "cobalt/script/callback_function.h"
#include "cobalt/script/script_value.h"

namespace cobalt {
namespace media_session {

class MediaSession : public script::Wrappable {
 public:
  typedef script::CallbackFunction<void()> MediaSessionActionHandler;
  typedef script::ScriptValue<MediaSessionActionHandler>
      MediaSessionActionHandlerHolder;
  enum MediaSessionPlaybackState { kNone, kPaused, kPlaying };

  enum MediaSessionAction {
    kPlay,
    kPause,
    kSeekbackward,
    kSeekforward,
    kPrevioustrack,
    kNexttrack
  };

  MediaSession() : state_(kNone) {}

  scoped_refptr<MediaMetadata> metadata() const { return metadata_; }

  void set_metadata(scoped_refptr<MediaMetadata> value) { metadata_ = value; }

  MediaSessionPlaybackState playback_state() const { return state_; }

  void set_playback_state(MediaSessionPlaybackState state) { state_ = state; }

  void SetActionHandler(MediaSessionAction action,
                        const MediaSessionActionHandlerHolder& handler) {
    UNREFERENCED_PARAMETER(action);
    UNREFERENCED_PARAMETER(handler);
    NOTIMPLEMENTED();
  }

  DEFINE_WRAPPABLE_TYPE(MediaSession);

 private:
  scoped_refptr<MediaMetadata> metadata_;
  MediaSessionPlaybackState state_;

  DISALLOW_COPY_AND_ASSIGN(MediaSession);
};

}  // namespace media_session
}  // namespace cobalt
#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_H_
