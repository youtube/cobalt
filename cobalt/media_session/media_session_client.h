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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_

#include <bitset>

#include "base/threading/thread_checker.h"

#include "cobalt/media_session/media_session.h"

namespace cobalt {
namespace media_session {

// Base class for a platform-level implementation of MediaSession.
// Platforms should subclass this to connect MediaSession to their platform.
class MediaSessionClient {
 public:
  typedef std::bitset<kMediaSessionActionNumActions> AvailableActionsSet;
  MediaSessionClient()
      : media_session_(new MediaSession(this)),
        platform_playback_state_(kMediaSessionPlaybackStateNone) {}

  virtual ~MediaSessionClient() {}

  // Retrieves the singleton MediaSession associated with this client.
  scoped_refptr<MediaSession>& GetMediaSession() { return media_session_; }

  // Retrieves the current actual playback state.
  // https://wicg.github.io/mediasession/#actual-playback-state
  // Must be called on the browser thread.
  MediaSessionPlaybackState GetActualPlaybackState();

  // Retrieves the set of currently available mediasession actions
  // per "media session actions update algorithm"
  // https://wicg.github.io/mediasession/#actions-model
  AvailableActionsSet GetAvailableActions();

  // Sets the platform's current playback state. This is used to compute
  // the "guessed playback state"
  // https://wicg.github.io/mediasession/#guessed-playback-state
  void UpdatePlatformPlaybackState(MediaSessionPlaybackState state);

  // Invokes a given media session action
  // https://wicg.github.io/mediasession/#actions-model
  void InvokeAction(MediaSessionAction action);

  // Invoked on the browser thread when any metadata, playback state,
  // or supported session actions change.
  virtual void OnMediaSessionChanged() = 0;

 private:
  base::ThreadChecker thread_checker_;
  scoped_refptr<MediaSession> media_session_;
  MediaSessionPlaybackState platform_playback_state_;
};

}  // namespace media_session
}  // namespace cobalt

#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_
