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

#include <pthread.h>

#include <limits>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_state.h"
#include "starboard/common/log.h"

namespace starboard {
namespace {

using base::android::ScopedJavaLocalRef;

// These constants are from android.media.session.PlaybackState
const jlong kPlaybackStateActionStop = 1 << 0;
const jlong kPlaybackStateActionPause = 1 << 1;
const jlong kPlaybackStateActionPlay = 1 << 2;
const jlong kPlaybackStateActionRewind = 1 << 3;
const jlong kPlaybackStateActionSkipToPrevious = 1 << 4;
const jlong kPlaybackStateActionSkipToNext = 1 << 5;
const jlong kPlaybackStateActionFastForward = 1 << 6;
const jlong kPlaybackStateActionSeekTo = 1 << 8;

// Converts a MediaSessionClient::AvailableActions bitset into
// a android.media.session.PlaybackState jlong bitset.
jlong MediaSessionActionsToPlaybackStateActions(const bool* actions) {
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

PlaybackState CobaltExtensionPlaybackStateToPlaybackState(
    CobaltExtensionMediaSessionPlaybackState in_state) {
  switch (in_state) {
    case kCobaltExtensionMediaSessionPlaying:
      return kPlaying;
    case kCobaltExtensionMediaSessionPaused:
      return kPaused;
    case kCobaltExtensionMediaSessionNone:
      return kNone;
  }
}

CobaltExtensionMediaSessionAction PlaybackStateActionToMediaSessionAction(
    jlong action) {
  CobaltExtensionMediaSessionAction result;
  switch (action) {
    case kPlaybackStateActionPause:
      result = kCobaltExtensionMediaSessionActionPause;
      break;
    case kPlaybackStateActionPlay:
      result = kCobaltExtensionMediaSessionActionPlay;
      break;
    case kPlaybackStateActionRewind:
      result = kCobaltExtensionMediaSessionActionSeekbackward;
      break;
    case kPlaybackStateActionSkipToPrevious:
      result = kCobaltExtensionMediaSessionActionPrevioustrack;
      break;
    case kPlaybackStateActionSkipToNext:
      result = kCobaltExtensionMediaSessionActionNexttrack;
      break;
    case kPlaybackStateActionFastForward:
      result = kCobaltExtensionMediaSessionActionSeekforward;
      break;
    case kPlaybackStateActionSeekTo:
      result = kCobaltExtensionMediaSessionActionSeekto;
      break;
    case kPlaybackStateActionStop:
      result = kCobaltExtensionMediaSessionActionStop;
      break;
    default:
      SB_NOTREACHED() << "Unsupported MediaSessionAction 0x" << std::hex
                      << action;
      result = static_cast<CobaltExtensionMediaSessionAction>(-1);
  }
  return result;
}

pthread_once_t once_flag = PTHREAD_ONCE_INIT;
pthread_mutex_t mutex;

// Callbacks to the last MediaSessionClient to become active, or null.
// Used to route Java callbacks.
// In practice, only one MediaSessionClient will become active at a time.
// Protected by "mutex"
CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
    g_update_platform_playback_state_callback = nullptr;
CobaltExtensionMediaSessionInvokeActionCallback g_invoke_action_callback =
    nullptr;
void* g_callback_context = nullptr;

void OnceInit() {
  pthread_mutex_init(&mutex, nullptr);
}

void NativeInvokeAction(jlong action, jlong seek_ms) {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  if (g_invoke_action_callback != nullptr && g_callback_context != nullptr) {
    CobaltExtensionMediaSessionActionDetails details = {};
    CobaltExtensionMediaSessionActionDetailsInit(
        &details, PlaybackStateActionToMediaSessionAction(action));
    // CobaltMediaSession.java only sets seek_ms for SeekTo (not ff/rew).
    if (details.action == kCobaltExtensionMediaSessionActionSeekto) {
      details.seek_time = seek_ms / 1000.0;
    }
    g_invoke_action_callback(details, g_callback_context);
  }

  pthread_mutex_unlock(&mutex);
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  if (g_update_platform_playback_state_callback != nullptr &&
      g_callback_context != nullptr) {
    g_update_platform_playback_state_callback(state, g_callback_context);
  }

  pthread_mutex_unlock(&mutex);
}

void OnMediaSessionStateChanged(
    const CobaltExtensionMediaSessionState session_state) {
  JNIEnv* env = base::android::AttachCurrentThread();

  jint playback_state = CobaltExtensionPlaybackStateToPlaybackState(
      session_state.actual_playback_state);

  jlong playback_state_actions = MediaSessionActionsToPlaybackStateActions(
      session_state.available_actions);

  ScopedJavaLocalRef<jstring> j_title;
  ScopedJavaLocalRef<jstring> j_artist;
  ScopedJavaLocalRef<jstring> j_album;
  ScopedJavaLocalRef<jobjectArray> j_artwork;

  if (session_state.metadata != nullptr) {
    CobaltExtensionMediaMetadata* media_metadata(session_state.metadata);

    j_title = ScopedJavaLocalRef<jstring>(
        env, JniNewStringStandardUTFOrAbort(env, media_metadata->title));
    j_artist = ScopedJavaLocalRef<jstring>(
        env, JniNewStringStandardUTFOrAbort(env, media_metadata->artist));
    j_album = ScopedJavaLocalRef<jstring>(
        env, JniNewStringStandardUTFOrAbort(env, media_metadata->album));

    size_t artwork_count = media_metadata->artwork_count;
    if (artwork_count > 0) {
      CobaltExtensionMediaImage* artwork(media_metadata->artwork);
      ScopedJavaLocalRef<jclass> media_image_class(
          env, JniFindClassExtOrAbort(env, "dev/cobalt/coat/MediaImage"));
      jmethodID media_image_constructor =
          env->GetMethodID(media_image_class.obj(), "<init>",
                           "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
                           "String;)V");
      JniAbortOnException(env);

      j_artwork = ScopedJavaLocalRef<jobjectArray>(
          env, static_cast<jobjectArray>(env->NewObjectArray(
                   artwork_count, media_image_class.obj(), nullptr)));
      JniAbortOnException(env);

      ScopedJavaLocalRef<jstring> j_src;
      ScopedJavaLocalRef<jstring> j_sizes;
      ScopedJavaLocalRef<jstring> j_type;
      for (size_t i = 0; i < artwork_count; i++) {
        const CobaltExtensionMediaImage& media_image(artwork[i]);
        j_src = ScopedJavaLocalRef<jstring>(
            env, JniNewStringStandardUTFOrAbort(env, media_image.src));
        j_sizes = ScopedJavaLocalRef<jstring>(
            env, JniNewStringStandardUTFOrAbort(env, media_image.size));
        j_type = ScopedJavaLocalRef<jstring>(
            env, JniNewStringStandardUTFOrAbort(env, media_image.type));

        ScopedJavaLocalRef<jobject> j_media_image(
            env,
            env->NewObject(media_image_class.obj(), media_image_constructor,
                           j_src.obj(), j_sizes.obj(), j_type.obj()));

        env->SetObjectArrayElement(j_artwork.obj(), i, j_media_image.obj());
      }
    }
  }

  jlong durationInMilliseconds;
  if (session_state.duration == std::numeric_limits<int64_t>::max()) {
    // Set duration to negative if duration is unknown or infinite, as with live
    // playback.
    // https://developer.android.com/reference/android/support/v4/media/MediaMetadataCompat#METADATA_KEY_DURATION
    durationInMilliseconds = -1;
  } else {
    // Starboard time is measured in microseconds while Android MediaSession
    // expects duration in milliseconds.
    durationInMilliseconds = session_state.duration / 1000;
  }

  JniCallVoidMethodOrAbort(
      env, JNIState::GetStarboardBridge(), "updateMediaSession",
      "(IJJFLjava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "[Ldev/cobalt/coat/MediaImage;J)V",
      playback_state, playback_state_actions,
      session_state.current_playback_position / 1000,
      static_cast<jfloat>(session_state.actual_playback_rate), j_title.obj(),
      j_artist.obj(), j_album.obj(), j_artwork.obj(), durationInMilliseconds);
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_platform_playback_state_callback) {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  g_callback_context = callback_context;
  g_invoke_action_callback = invoke_action_callback;
  g_update_platform_playback_state_callback =
      update_platform_playback_state_callback;

  pthread_mutex_unlock(&mutex);
}

void DestroyMediaSessionClientCallback() {
  pthread_once(&once_flag, OnceInit);
  pthread_mutex_lock(&mutex);

  g_callback_context = nullptr;
  g_invoke_action_callback = nullptr;
  g_update_platform_playback_state_callback = nullptr;

  pthread_mutex_unlock(&mutex);

  JNIEnv* env = base::android::AttachCurrentThread();
  JniCallVoidMethodOrAbort(env, JNIState::GetStarboardBridge(),
                           "deactivateMediaSession", "()V");
}

}  // namespace

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

}  // namespace starboard

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_coat_CobaltMediaSession_nativeInvokeAction(JNIEnv* env,
                                                           jclass unused_clazz,
                                                           jlong action,
                                                           jlong seek_ms) {
  starboard::NativeInvokeAction(action, seek_ms);
}
