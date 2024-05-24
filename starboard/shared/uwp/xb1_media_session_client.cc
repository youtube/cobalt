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

#include "starboard/shared/uwp/xb1_media_session_client.h"

#include <pthread.h>

#include "starboard/common/log.h"
#include "starboard/common/semaphore.h"
#include "starboard/key.h"
#include "starboard/shared/uwp/app_accessors.h"

using starboard::shared::uwp::DisplayRequestActive;
using starboard::shared::uwp::DisplayRequestRelease;
using starboard::shared::uwp::GetTransportControls;
using starboard::shared::uwp::InjectKeypress;
using Windows::Foundation::TypedEventHandler;
using Windows::Media::MediaPlaybackStatus;
using Windows::Media::SystemMediaTransportControls;
using Windows::Media::SystemMediaTransportControlsButton;
using Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs;
using Windows::Media::Playback::MediaPlayer;
using Windows::UI::Core::CoreDispatcherPriority;
using Windows::UI::Core::DispatchedHandler;

namespace {

MediaPlaybackStatus MediaSessionPlaybackStateToMediaPlaybackState(
    CobaltExtensionMediaSessionPlaybackState playback_state) {
  switch (playback_state) {
    case kCobaltExtensionMediaSessionNone:
      return MediaPlaybackStatus::Stopped;
    case kCobaltExtensionMediaSessionPaused:
      return MediaPlaybackStatus::Paused;
    case kCobaltExtensionMediaSessionPlaying:
      return MediaPlaybackStatus::Playing;
    default:
      SB_NOTREACHED() << "Invalid playback_state " << playback_state;
  }
  return MediaPlaybackStatus::Closed;
}

pthread_once_t once_flag = PTHREAD_ONCE_INIT;
pthread_mutex_t mutex;

// Callbacks to the last MediaSessionClient to become active, or null.
// In practice, only one MediaSessionClient will become active at a time.
// Protected by "mutex"
CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
    g_update_platform_playback_state_callback;
void* g_callback_context;

bool button_press_callback_registered = false;
bool active = false;
bool media_playing = false;

void OnceInit() {
  pthread_mutex_init(&mutex, nullptr);
}

void InitButtonCallbackOnce() {
  if (button_press_callback_registered) {
    return;
  }
  button_press_callback_registered = true;
  Platform::Agile<SystemMediaTransportControls> transport_controls =
      GetTransportControls();

  transport_controls->ButtonPressed += ref new TypedEventHandler<
      SystemMediaTransportControls ^,
      SystemMediaTransportControlsButtonPressedEventArgs ^>(
      [](SystemMediaTransportControls ^ controls,
         SystemMediaTransportControlsButtonPressedEventArgs ^ args) {
        SbKey key_code;
        switch (args->Button) {
          case SystemMediaTransportControlsButton::ChannelDown:
            key_code = kSbKeyChannelDown;
            break;
          case SystemMediaTransportControlsButton::ChannelUp:
            key_code = kSbKeyChannelUp;
            break;
          case SystemMediaTransportControlsButton::Next:
            key_code = kSbKeyMediaNextTrack;
            break;
          case SystemMediaTransportControlsButton::Pause:
          case SystemMediaTransportControlsButton::Play:
            key_code = kSbKeyMediaPlayPause;
            break;
          case SystemMediaTransportControlsButton::Previous:
            key_code = kSbKeyMediaPrevTrack;
            break;
          case SystemMediaTransportControlsButton::Stop:
            key_code = kSbKeyMediaStop;
            break;
          case SystemMediaTransportControlsButton::FastForward:
            key_code = kSbKeyMediaFastForward;
            break;
          case SystemMediaTransportControlsButton::Rewind:
            key_code = kSbKeyMediaRewind;
            break;
          default:
            key_code = kSbKeyUnknown;
            break;
        }
        if (key_code != kSbKeyUnknown) {
          InjectKeypress(key_code);
        }
      });
}

void OnMediaSessionStateChanged(
    const CobaltExtensionMediaSessionState session_state) {
  const CobaltExtensionMediaSessionPlaybackState playback_state =
      session_state.actual_playback_state;

  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  InitButtonCallbackOnce();
  Platform::Agile<SystemMediaTransportControls> transport_controls =
      GetTransportControls();

  pthread_mutex_unlock(&mutex);

  const bool sessionActive = kCobaltExtensionMediaSessionNone == playback_state;

  bool now_active = false;
  bool now_media_playing = false;

  switch (playback_state) {
    case kCobaltExtensionMediaSessionPlaying:
      now_active = true;
      now_media_playing = true;
      break;
    case kCobaltExtensionMediaSessionPaused:
      now_active = true;
      now_media_playing = false;
      break;
    case kCobaltExtensionMediaSessionNone:
      now_active = false;
      now_media_playing = false;
      break;
    default:
      SB_NOTREACHED() << "Unknown state reached " << playback_state;
  }
  if (now_media_playing && !media_playing) {
    DisplayRequestActive();
  } else if (!now_media_playing && media_playing) {
    DisplayRequestRelease();
  }

  transport_controls->IsEnabled = now_active;

  active = now_active;
  media_playing = now_media_playing;

  transport_controls->IsChannelDownEnabled = now_active;
  transport_controls->IsChannelUpEnabled = now_active;
  transport_controls->IsFastForwardEnabled = now_active;
  transport_controls->IsNextEnabled = now_active;
  transport_controls->IsPauseEnabled = now_active;
  transport_controls->IsPlayEnabled = now_active;
  transport_controls->IsPreviousEnabled = now_active;
  transport_controls->IsRecordEnabled = now_active;
  transport_controls->IsRewindEnabled = now_active;
  transport_controls->IsStopEnabled = now_active;

  if (!active) {
    return;
  }

  transport_controls->PlaybackStatus =
      MediaSessionPlaybackStateToMediaPlaybackState(playback_state);
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_platform_playback_state_callback) {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  g_callback_context = callback_context;
  g_update_platform_playback_state_callback =
      update_platform_playback_state_callback;

  pthread_mutex_unlock(&mutex);
}

void DestroyMediaSessionClientCallback() {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  g_callback_context = NULL;
  g_update_platform_playback_state_callback = NULL;

  pthread_mutex_unlock(&mutex);
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  if (g_update_platform_playback_state_callback != NULL &&
      g_callback_context != NULL) {
    g_update_platform_playback_state_callback(state, g_callback_context);
  }

  pthread_mutex_unlock(&mutex);
}
}  // namespace

namespace starboard {
namespace shared {
namespace uwp {

const CobaltExtensionMediaSessionApi kMediaSessionApi = {
    kCobaltExtensionMediaSessionName,
    1,
    &OnMediaSessionStateChanged,
    &RegisterMediaSessionCallbacks,
    &DestroyMediaSessionClientCallback,
    &UpdateActiveSessionPlatformPlaybackState};

const void* GetMediaSessionApi() {
  return &kMediaSessionApi;
}
}  // namespace uwp
}  // namespace shared
}  // namespace starboard
