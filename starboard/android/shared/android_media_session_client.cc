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

#include "base/time/time.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/once.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::ScopedLocalJavaRef;

// These constants are from android.media.session.PlaybackState
const jlong kPlaybackStateActionStop = 1 << 0;
const jlong kPlaybackStateActionPause = 1 << 1;
const jlong kPlaybackStateActionPlay = 1 << 2;
const jlong kPlaybackStateActionRewind = 1 << 3;
const jlong kPlaybackStateActionSkipToPrevious = 1 << 4;
const jlong kPlaybackStateActionSkipToNext = 1 << 5;
const jlong kPlaybackStateActionFastForward = 1 << 6;
const jlong kPlaybackStateActionSetRating = 1 << 7;  // not supported
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

SbOnceControl once_flag = SB_ONCE_INITIALIZER;
SbMutex mutex;

// Callbacks to the last MediaSessionClient to become active, or null.
// Used to route Java callbacks.
// In practice, only one MediaSessionClient will become active at a time.
// Protected by "mutex"
CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
    g_update_platform_playback_state_callback;
CobaltExtensionMediaSessionInvokeActionCallback g_invoke_action_callback;
void* g_callback_context;

void OnceInit() {
  SbMutexCreate(&mutex);
}

void NativeInvokeAction(jlong action, jlong seek_ms) {
  SbOnce(&once_flag, OnceInit);
  SbMutexAcquire(&mutex);

  if (g_invoke_action_callback != NULL && g_callback_context != NULL) {
    CobaltExtensionMediaSessionActionDetails details = {};
    CobaltExtensionMediaSessionActionDetailsInit(
        &details, PlaybackStateActionToMediaSessionAction(action));
    // CobaltMediaSession.java only sets seek_ms for SeekTo (not ff/rew).
    if (details.action == kCobaltExtensionMediaSessionActionSeekto) {
      details.seek_time = seek_ms / 1000.0;
    }
    g_invoke_action_callback(details, g_callback_context);
  }

  SbMutexRelease(&mutex);
}

void UpdateActiveSessionPlatformPlaybackState(
    CobaltExtensionMediaSessionPlaybackState state) {
  SbOnce(&once_flag, OnceInit);
  SbMutexAcquire(&mutex);

  if (g_update_platform_playback_state_callback != NULL &&
      g_callback_context != NULL) {
    g_update_platform_playback_state_callback(state, g_callback_context);
  }

  SbMutexRelease(&mutex);
}

void OnMediaSessionStateChanged(
    const CobaltExtensionMediaSessionState session_state) {
  JniEnvExt* env = JniEnvExt::Get();

  jint playback_state = CobaltExtensionPlaybackStateToPlaybackState(
      session_state.actual_playback_state);

  SbOnce(&once_flag, OnceInit);
  SbMutexAcquire(&mutex);

  SbMutexRelease(&mutex);

  jlong playback_state_actions = MediaSessionActionsToPlaybackStateActions(
      session_state.available_actions);

  ScopedLocalJavaRef<jstring> j_title;
  ScopedLocalJavaRef<jstring> j_artist;
  ScopedLocalJavaRef<jstring> j_album;
  ScopedLocalJavaRef<jobjectArray> j_artwork;

  if (session_state.metadata != NULL) {
    CobaltExtensionMediaMetadata* media_metadata(session_state.metadata);

    j_title.Reset(env->NewStringStandardUTFOrAbort(media_metadata->title));
    j_artist.Reset(env->NewStringStandardUTFOrAbort(media_metadata->artist));
    j_album.Reset(env->NewStringStandardUTFOrAbort(media_metadata->album));

    size_t artwork_count = media_metadata->artwork_count;
    if (artwork_count > 0) {
      CobaltExtensionMediaImage* artwork(media_metadata->artwork);
      ScopedLocalJavaRef<jclass> media_image_class(
          env->FindClassExtOrAbort("dev/cobalt/media/MediaImage"));
      jmethodID media_image_constructor = env->GetMethodID(
          media_image_class.Get(), "<init>",
          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
      env->AbortOnException();

      j_artwork.Reset(static_cast<jobjectArray>(
          env->NewObjectArray(artwork_count, media_image_class.Get(), NULL)));
      env->AbortOnException();

      ScopedLocalJavaRef<jstring> j_src;
      ScopedLocalJavaRef<jstring> j_sizes;
      ScopedLocalJavaRef<jstring> j_type;
      for (size_t i = 0; i < artwork_count; i++) {
        const CobaltExtensionMediaImage& media_image(artwork[i]);
        j_src.Reset(env->NewStringStandardUTFOrAbort(media_image.src));
        j_sizes.Reset(env->NewStringStandardUTFOrAbort(media_image.size));
        j_type.Reset(env->NewStringStandardUTFOrAbort(media_image.type));

        ScopedLocalJavaRef<jobject> j_media_image(
            env->NewObject(media_image_class.Get(), media_image_constructor,
                           j_src.Get(), j_sizes.Get(), j_type.Get()));

        env->SetObjectArrayElement(j_artwork.Get(), i, j_media_image.Get());
      }
    }
  }

  jlong durationInMilliseconds;
  if (session_state.duration == kSbTimeMax) {
    // Set duration to negative if duration is unknown or infinite, as with live
    // playback.
    // https://developer.android.com/reference/android/support/v4/media/MediaMetadataCompat#METADATA_KEY_DURATION
    durationInMilliseconds = -1;
  } else {
    // SbTime is measured in microseconds while Android MediaSession expects
    // duration in milliseconds.
    durationInMilliseconds = session_state.duration / kSbTimeMillisecond;
  }

  env->CallStarboardVoidMethodOrAbort(
      "updateMediaSession",
      "(IJJFLjava/lang/String;Ljava/lang/String;Ljava/lang/String;"
      "[Ldev/cobalt/media/MediaImage;J)V",
      playback_state, playback_state_actions,
      session_state.current_playback_position / kSbTimeMillisecond,
      static_cast<jfloat>(session_state.actual_playback_rate), j_title.Get(),
      j_artist.Get(), j_album.Get(), j_artwork.Get(), durationInMilliseconds);
}

void RegisterMediaSessionCallbacks(
    void* callback_context,
    CobaltExtensionMediaSessionInvokeActionCallback invoke_action_callback,
    CobaltExtensionMediaSessionUpdatePlatformPlaybackStateCallback
        update_platform_playback_state_callback) {
  SbOnce(&once_flag, OnceInit);
  SbMutexAcquire(&mutex);

  g_callback_context = callback_context;
  g_invoke_action_callback = invoke_action_callback;
  g_update_platform_playback_state_callback =
      update_platform_playback_state_callback;

  SbMutexRelease(&mutex);
}

}  // namespace

const CobaltExtensionMediaSessionApi kMediaSessionApi = {
    kCobaltExtensionMediaSessionName,
    1,
    &OnMediaSessionStateChanged,
    &RegisterMediaSessionCallbacks,
    NULL,
    &UpdateActiveSessionPlatformPlaybackState};

const void* GetMediaSessionApi() {
  return &kMediaSessionApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

extern "C" SB_EXPORT_PLATFORM void
Java_dev_cobalt_media_CobaltMediaSession_nativeInvokeAction(JNIEnv* env,
                                                            jclass unused_clazz,
                                                            jlong action,
                                                            jlong seek_ms) {
  starboard::android::shared::NativeInvokeAction(action, seek_ms);
}
