// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// The AOSP MediaSession extension. Cobalt reports the web app's media session
// through it, and StarboardMediaSession.java publishes that as an Android
// MediaSession. Android TV builds use CobaltMediaSession.java instead.

#include <jni.h>

#include <limits>
#include <mutex>

#include "base/android/jni_string.h"
#include "starboard/android/shared/android_media_session_client.h"
#include "starboard/common/log.h"
#include "third_party/jni_zero/jni_zero.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "cobalt/android/jni_headers/StarboardMediaSession_jni.h"

namespace starboard {
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

// Converts the extension's available actions into an
// android.media.session.PlaybackState actions bitset.
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
      return kNone;
  }
  return kNone;
}

bool ToMediaSessionAction(jlong action,
                          CobaltExtensionMediaSessionAction* result) {
  switch (action) {
    case kPlaybackStateActionPause:
      *result = kCobaltExtensionMediaSessionActionPause;
      return true;
    case kPlaybackStateActionPlay:
      *result = kCobaltExtensionMediaSessionActionPlay;
      return true;
    case kPlaybackStateActionRewind:
      *result = kCobaltExtensionMediaSessionActionSeekbackward;
      return true;
    case kPlaybackStateActionSkipToPrevious:
      *result = kCobaltExtensionMediaSessionActionPrevioustrack;
      return true;
    case kPlaybackStateActionSkipToNext:
      *result = kCobaltExtensionMediaSessionActionNexttrack;
      return true;
    case kPlaybackStateActionFastForward:
      *result = kCobaltExtensionMediaSessionActionSeekforward;
      return true;
    case kPlaybackStateActionSeekTo:
      *result = kCobaltExtensionMediaSessionActionSeekto;
      return true;
    case kPlaybackStateActionStop:
      *result = kCobaltExtensionMediaSessionActionStop;
      return true;
  }
  return false;
}

// The callbacks of the Cobalt media session client that registered last.
struct Callbacks {
  void* context = nullptr;
  CobaltExtensionMediaSessionInvokeActionCallback invoke_action = nullptr;
  CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
      update_platform_playback_state = nullptr;
};

std::mutex& GetMutex() {
  static std::mutex mutex;
  return mutex;
}

Callbacks& GetCallbacks() {
  static Callbacks callbacks;
  return callbacks;
}

base::android::ScopedJavaLocalRef<jstring> ToJavaString(JNIEnv* env,
                                                        const char* str) {
  return base::android::ConvertUTF8ToJavaString(env, str ? str : "");
}

void OnMediaSessionStateChanged(CobaltExtensionMediaSessionState state) {
  JNIEnv* env = jni_zero::AttachCurrentThread();

  const CobaltExtensionMediaMetadata* metadata = state.metadata;
  // Android expects a negative duration when it's unknown or infinite, as for
  // live streams.
  jlong duration_ms = -1;
  jlong position_ms = -1;
  if (state.has_position_state) {
    if (state.duration != std::numeric_limits<int64_t>::max()) {
      duration_ms = state.duration / 1000;
    }
    position_ms = state.current_playback_position / 1000;
  }
  // Pass only the last artwork entry. Android takes a single image URI.
  const char* artwork_src = nullptr;
  if (metadata && metadata->artwork && metadata->artwork_count > 0) {
    artwork_src = metadata->artwork[metadata->artwork_count - 1].src;
  }

  Java_StarboardMediaSession_updateMediaSession(
      env, ToPlaybackState(state.actual_playback_state),
      ToPlaybackStateActions(state.available_actions), position_ms,
      static_cast<jfloat>(state.actual_playback_rate),
      ToJavaString(env, metadata ? metadata->title : nullptr),
      ToJavaString(env, metadata ? metadata->artist : nullptr),
      ToJavaString(env, metadata ? metadata->album : nullptr),
      ToJavaString(env, artwork_src), duration_ms);
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_platform_playback_state_callback) {
  std::lock_guard lock(GetMutex());
  Callbacks& callbacks = GetCallbacks();
  callbacks.context = callback_context;
  callbacks.invoke_action = invoke_action_callback;
  callbacks.update_platform_playback_state =
      update_platform_playback_state_callback;
}

void DestroyMediaSessionClientCallback() {
  {
    std::lock_guard lock(GetMutex());
    GetCallbacks() = Callbacks();
  }
  Java_StarboardMediaSession_deactivateMediaSession(
      jni_zero::AttachCurrentThread());
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  std::lock_guard lock(GetMutex());
  const Callbacks& callbacks = GetCallbacks();
  if (callbacks.update_platform_playback_state && callbacks.context) {
    callbacks.update_platform_playback_state(state, callbacks.context);
  }
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

void JNI_StarboardMediaSession_InvokeAction(JNIEnv* env,
                                            jlong action,
                                            jlong seek_ms) {
  CobaltExtensionMediaSessionActionDetails details;
  CobaltExtensionMediaSessionAction media_session_action;
  if (!ToMediaSessionAction(action, &media_session_action)) {
    SB_LOG(WARNING) << "Unsupported MediaSession action " << action;
    return;
  }
  CobaltExtensionMediaSessionActionDetailsInit(&details, media_session_action);
  if (details.action == kCobaltExtensionMediaSessionActionSeekto) {
    details.seek_time = seek_ms / 1000.0;
  }

  std::lock_guard lock(GetMutex());
  const Callbacks& callbacks = GetCallbacks();
  if (callbacks.invoke_action && callbacks.context) {
    callbacks.invoke_action(details, callbacks.context);
  }
}

}  // namespace starboard
