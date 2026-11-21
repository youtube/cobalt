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

#include "starboard/android/shared/android_media_session_client.h"

#include "build/build_config.h"
#include "starboard/common/log.h"

#if BUILDFLAG(IS_STARBOARD)
#include <jni.h>

#include <cstdint>
#include <limits>
#include <mutex>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardMediaSession_jni.h"
#endif  // BUILDFLAG(IS_STARBOARD)

namespace starboard {

#if BUILDFLAG(IS_STARBOARD)
// AOSP (Starboard) build: the prebuilt Evergreen Core reports the web app's
// media session through this extension (see
// cobalt/browser/media_session/media_session_extension_bridge.h), and
// dev.cobalt.coat.StarboardMediaSession mirrors it into an Android
// MediaSession so system controllers such as Alexa can control playback.
namespace {

// These constants are from android.media.session.PlaybackState.
constexpr jlong kPlaybackStateActionStop = 1 << 0;
constexpr jlong kPlaybackStateActionPause = 1 << 1;
constexpr jlong kPlaybackStateActionPlay = 1 << 2;
constexpr jlong kPlaybackStateActionRewind = 1 << 3;
constexpr jlong kPlaybackStateActionSkipToPrevious = 1 << 4;
constexpr jlong kPlaybackStateActionSkipToNext = 1 << 5;
constexpr jlong kPlaybackStateActionFastForward = 1 << 6;
constexpr jlong kPlaybackStateActionSeekTo = 1 << 8;
constexpr jlong kPlaybackStateActionPlayPause = 1 << 9;

// Converts the extension's available actions into an
// android.media.session.PlaybackState action bitmask.
jlong ToPlaybackStateActions(const bool* actions) {
  jlong result = 0;
  if (actions[kCobaltExtensionMediaSessionActionPause]) {
    result |= kPlaybackStateActionPause;
  }
  if (actions[kCobaltExtensionMediaSessionActionPlay]) {
    result |= kPlaybackStateActionPlay;
  }
  if (actions[kCobaltExtensionMediaSessionActionSeekbackward]) {
    result |= kPlaybackStateActionRewind;
  }
  if (actions[kCobaltExtensionMediaSessionActionPrevioustrack]) {
    result |= kPlaybackStateActionSkipToPrevious;
  }
  if (actions[kCobaltExtensionMediaSessionActionNexttrack]) {
    result |= kPlaybackStateActionSkipToNext;
  }
  if (actions[kCobaltExtensionMediaSessionActionSeekforward]) {
    result |= kPlaybackStateActionFastForward;
  }
  if (actions[kCobaltExtensionMediaSessionActionSeekto]) {
    result |= kPlaybackStateActionSeekTo;
  }
  if (actions[kCobaltExtensionMediaSessionActionStop]) {
    result |= kPlaybackStateActionStop;
  }
  return result;
}

PlaybackState ToPlaybackState(CobaltExtensionMediaSessionPlaybackState state) {
  switch (state) {
    case kCobaltExtensionMediaSessionPlaying:
      return kPlaying;
    case kCobaltExtensionMediaSessionPaused:
      return kPaused;
    case kCobaltExtensionMediaSessionNone:
    default:
      return kNone;
  }
}

// Maps an android.media.session.PlaybackState action to the extension's
// action. Returns false for unsupported actions.
bool ToExtensionAction(jlong action, CobaltExtensionMediaSessionAction* out) {
  switch (action) {
    case kPlaybackStateActionPause:
      *out = kCobaltExtensionMediaSessionActionPause;
      return true;
    case kPlaybackStateActionPlay:
      *out = kCobaltExtensionMediaSessionActionPlay;
      return true;
    case kPlaybackStateActionRewind:
      *out = kCobaltExtensionMediaSessionActionSeekbackward;
      return true;
    case kPlaybackStateActionSkipToPrevious:
      *out = kCobaltExtensionMediaSessionActionPrevioustrack;
      return true;
    case kPlaybackStateActionSkipToNext:
      *out = kCobaltExtensionMediaSessionActionNexttrack;
      return true;
    case kPlaybackStateActionFastForward:
      *out = kCobaltExtensionMediaSessionActionSeekforward;
      return true;
    case kPlaybackStateActionSeekTo:
      *out = kCobaltExtensionMediaSessionActionSeekto;
      return true;
    case kPlaybackStateActionStop:
      *out = kCobaltExtensionMediaSessionActionStop;
      return true;
    default:
      return false;
  }
}

// Callbacks registered by the web engine. The extension is a process-wide
// singleton, so only one registration exists at a time.
std::mutex& GetMutex() {
  static std::mutex* mutex = new std::mutex();
  return *mutex;
}
void* g_callback_context = nullptr;
CobaltExtensionMediaSessionInvokeActionCallback g_invoke_action_callback =
    nullptr;

jni_zero::ScopedJavaLocalRef<jstring> ToJavaStringOrNull(JNIEnv* env,
                                                         const char* str) {
  if (!str || !*str) {
    return nullptr;
  }
  return base::android::ConvertUTF8ToJavaString(env, str);
}

void OnMediaSessionStateChanged(CobaltExtensionMediaSessionState state) {
  JNIEnv* env = base::android::AttachCurrentThread();

  jni_zero::ScopedJavaLocalRef<jstring> j_title;
  jni_zero::ScopedJavaLocalRef<jstring> j_artist;
  jni_zero::ScopedJavaLocalRef<jstring> j_album;
  jni_zero::ScopedJavaLocalRef<jstring> j_artwork;
  if (state.metadata) {
    j_title = ToJavaStringOrNull(env, state.metadata->title);
    j_artist = ToJavaStringOrNull(env, state.metadata->artist);
    j_album = ToJavaStringOrNull(env, state.metadata->album);
    // Pass the last artwork entry; web apps typically list artwork from
    // smallest to largest.
    if (state.metadata->artwork_count > 0 && state.metadata->artwork) {
      j_artwork = ToJavaStringOrNull(
          env, state.metadata->artwork[state.metadata->artwork_count - 1].src);
    }
  }

  // Starboard time is in microseconds; Android MediaSession uses milliseconds
  // and a negative duration for unknown/live media.
  jlong duration_ms = -1;
  if (state.has_position_state && state.duration > 0 &&
      state.duration != std::numeric_limits<int64_t>::max()) {
    duration_ms = state.duration / 1000;
  }
  jlong position_ms =
      state.has_position_state ? state.current_playback_position / 1000 : 0;

  Java_StarboardMediaSession_updateMediaSession(
      env, static_cast<jint>(ToPlaybackState(state.actual_playback_state)),
      ToPlaybackStateActions(state.available_actions), position_ms,
      static_cast<jfloat>(state.actual_playback_rate), j_title, j_artist,
      j_album, j_artwork, duration_ms);
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
    /*update_platform_playback_state_callback*/) {
  std::lock_guard<std::mutex> lock(GetMutex());
  g_callback_context = callback_context;
  g_invoke_action_callback = invoke_action_callback;
  SB_LOG(INFO) << "MediaSession callbacks registered.";
}

void DestroyMediaSessionClientCallback() {
  {
    std::lock_guard<std::mutex> lock(GetMutex());
    g_callback_context = nullptr;
    g_invoke_action_callback = nullptr;
  }
  Java_StarboardMediaSession_deactivateMediaSession(
      base::android::AttachCurrentThread());
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState /*state*/) {
  // SbPlayer calls this (via starboard/shared/media_session) whenever a player
  // is created, destroyed or changes rate. The web engine already tracks
  // player state itself and reports it in OnMediaSessionStateChanged(), so
  // nothing needs to be forwarded.
}

const CobaltExtensionMediaSessionApi kMediaSessionApi = {
    kCobaltExtensionMediaSessionName,
    1,
    &OnMediaSessionStateChanged,
    &RegisterMediaSessionCallbacks,
    &DestroyMediaSessionClientCallback,
    &UpdateActiveSessionPlatformPlaybackState,
};

}  // namespace

const void* GetMediaSessionApi() {
  return &kMediaSessionApi;
}

// Called by StarboardMediaSession.java when a system controller (remote,
// voice assistant, launcher) sends a transport control.
void JNI_StarboardMediaSession_InvokeAction(JNIEnv* env,
                                            jlong action,
                                            jlong seek_ms) {
  CobaltExtensionMediaSessionActionDetails details;
  if (action == kPlaybackStateActionPlayPause) {
    // MediaSessionCompat normally resolves PLAY_PAUSE into onPlay()/onPause().
    SB_LOG(WARNING) << "Ignoring unresolved PLAY_PAUSE media session action.";
    return;
  }
  CobaltExtensionMediaSessionAction ext_action;
  if (!ToExtensionAction(action, &ext_action)) {
    SB_LOG(WARNING) << "Unsupported media session action 0x" << std::hex
                    << action;
    return;
  }
  CobaltExtensionMediaSessionActionDetailsInit(&details, ext_action);
  if (ext_action == kCobaltExtensionMediaSessionActionSeekto) {
    details.seek_time = seek_ms / 1000.0;
  }

  // Holding the lock while invoking ensures the context is not used after
  // DestroyMediaSessionClientCallback() returns. The web engine's callback
  // only posts a task, so this does not block for long.
  std::lock_guard<std::mutex> lock(GetMutex());
  if (!g_invoke_action_callback || !g_callback_context) {
    SB_LOG(WARNING) << "Dropping media session action " << ext_action
                    << ": no web engine callback registered.";
    return;
  }
  g_invoke_action_callback(details, g_callback_context);
}

#else  // BUILDFLAG(IS_STARBOARD)

const void* GetMediaSessionApi() {
  // The Android TV build uses the Chromium MediaSession implementation
  // directly (CobaltMediaSession.java), bypassing this Starboard extension.
  SB_LOG(FATAL) << "MediaSessionApi is not supported.";
  return nullptr;
}

#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace starboard
