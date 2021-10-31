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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_

#include <bitset>
#include <memory>

#include "base/threading/thread_checker.h"
#include "cobalt/extension/media_session.h"
#include "cobalt/media/web_media_player_factory.h"
#include "cobalt/media_session/media_session.h"
#include "cobalt/media_session/media_session_action_details.h"
#include "cobalt/media_session/media_session_state.h"
#include "starboard/time.h"

namespace cobalt {
namespace media_session {

// Base class for a platform-level implementation of MediaSession.
// Platforms should subclass this to connect MediaSession to their platform.
class MediaSessionClient {
  friend class MediaSession;

 public:
  MediaSessionClient(): MediaSessionClient(nullptr) {}
  // Injectable MediaSession for tests.
  explicit MediaSessionClient(MediaSession* media_session);

  virtual ~MediaSessionClient();

  // Retrieves the singleton MediaSession associated with this client.
  MediaSession* GetMediaSession() { return media_session_; }

  // The web app should set the MediaPositionState of the MediaSession object.
  // However, if that is not done, then query the web media player factory to
  // guess which player is associated with the media session to get the media
  // position state. The player factory must outlive the media session client.
  void SetMediaPlayerFactory(const media::WebMediaPlayerFactory* factory);

  // Sets the platform's current playback state. This is used to compute
  // the "guessed playback state"
  // https://wicg.github.io/mediasession/#guessed-playback-state
  // Can be invoked from any thread.
  void UpdatePlatformPlaybackState(
      CobaltExtensionMediaSessionPlaybackState state);

  // Invokes a given media session action
  // https://wicg.github.io/mediasession/#actions-model
  // Can be invoked from any thread.
  void InvokeAction(CobaltExtensionMediaSessionAction action) {
    std::unique_ptr<CobaltExtensionMediaSessionActionDetails> details(
        new CobaltExtensionMediaSessionActionDetails());
    CobaltExtensionMediaSessionActionDetailsInit(details.get(), action);
    InvokeActionInternal(std::move(details));
  }

  // Invokes a given media session action that takes additional details.
  void InvokeAction(CobaltExtensionMediaSessionActionDetails details) {
    std::unique_ptr<CobaltExtensionMediaSessionActionDetails> details_ptr(
        new CobaltExtensionMediaSessionActionDetails(details));
    InvokeActionInternal(std::move(details_ptr));
  }

  // Invoked on the browser thread when any metadata, position state, playback
  // state, or supported session actions change.
  virtual void OnMediaSessionStateChanged(
      const MediaSessionState& session_state);

  // Indicate the media session client is active or not depending on the
  // media session playback state.
  bool is_active() {
    return session_state_.actual_playback_state() !=
        kMediaSessionPlaybackStateNone;
  }

  // Set maybe freeze callback.
  void SetMaybeFreezeCallback(const base::Closure& maybe_freeze_callback) {
    maybe_freeze_callback_ = maybe_freeze_callback;
  }

  void set_media_session(MediaSession* media_session) {
    media_session_ = media_session;
  }

  // Post a delayed task for running MaybeFreeze callback.
  void PostDelayedTaskForMaybeFreezeCallback();

 private:
  THREAD_CHECKER(thread_checker_);
  MediaSession* media_session_;
  MediaSessionState session_state_;
  MediaSessionPlaybackState platform_playback_state_;
  const media::WebMediaPlayerFactory* media_player_factory_ = nullptr;
  const CobaltExtensionMediaSessionApi* extension_;

  void UpdateMediaSessionState();
  MediaSessionPlaybackState ComputeActualPlaybackState() const;
  MediaSessionState::AvailableActionsSet ComputeAvailableActions() const;
  void InvokeActionInternal(
      std::unique_ptr<CobaltExtensionMediaSessionActionDetails> details);

  void ConvertMediaSessionActions(
      const MediaSessionState::AvailableActionsSet& actions,
      bool result[kCobaltExtensionMediaSessionActionNumActions]);
  std::unique_ptr<MediaSessionActionDetails> ConvertActionDetails(
      const CobaltExtensionMediaSessionActionDetails& ext_details);

  // Static callback wrappers for MediaSessionAPI extension.
  static void UpdatePlatformPlaybackStateCallback(
      CobaltExtensionMediaSessionPlaybackState state, void* callback_context);
  static void InvokeActionCallback(
      CobaltExtensionMediaSessionActionDetails details, void* callback_context);

  // MediaSessionAPI extension type conversion helpers.
  CobaltExtensionMediaSessionPlaybackState ConvertPlaybackState(
      MediaSessionPlaybackState state);
  MediaSessionPlaybackState ConvertPlaybackState(
      CobaltExtensionMediaSessionPlaybackState state);
  CobaltExtensionMediaSessionAction ConvertMediaSessionAction(
      MediaSessionAction action);
  MediaSessionAction ConvertMediaSessionAction(
      CobaltExtensionMediaSessionAction action);

  // If the media session is not active, then run MaybeFreezeCallback to
  // suspend the App.
  void RunMaybeFreezeCallback(int sequence_number);

  base::Closure maybe_freeze_callback_;

  // This is for checking the sequence number of PostDelayedTask. It should be
  // aligned with a single thread.
  int sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(MediaSessionClient);
};

}  // namespace media_session
}  // namespace cobalt

#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_CLIENT_H_
