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

#include "starboard/android/shared/cobalt/android_media_session_client.h"

#include "base/time.h"
#include "cobalt/media_session/media_session_client.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/log.h"
#include "starboard/mutex.h"
#include "starboard/once.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

using ::cobalt::media_session::MediaMetadata;
using ::cobalt::media_session::MediaSession;
using ::cobalt::media_session::MediaSessionAction;
using ::cobalt::media_session::MediaSessionClient;
using ::cobalt::media_session::MediaSessionPlaybackState;
using ::cobalt::media_session::kMediaSessionActionPause;
using ::cobalt::media_session::kMediaSessionActionPlay;
using ::cobalt::media_session::kMediaSessionActionSeekbackward;
using ::cobalt::media_session::kMediaSessionActionSeekforward;
using ::cobalt::media_session::kMediaSessionActionPrevioustrack;
using ::cobalt::media_session::kMediaSessionActionNexttrack;
using ::cobalt::media_session::kMediaSessionPlaybackStateNone;
using ::cobalt::media_session::kMediaSessionPlaybackStatePaused;
using ::cobalt::media_session::kMediaSessionPlaybackStatePlaying;

using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::ScopedLocalJavaRef;

namespace {

// These constants are from android.media.session.PlaybackState
const jlong kPlaybackStateActionPause = 1 << 1;
const jlong kPlaybackStateActionPlay = 1 << 2;
const jlong kPlaybackStateActionRewind = 1 << 3;
const jlong kPlaybackStateActionSkipToPrevious = 1 << 4;
const jlong kPlaybackStateActionSkipToNext = 1 << 5;
const jlong kPlaybackStateActionFastForward = 1 << 6;

// Converts a MediaSessionClient::AvailableActions bitset into
// a android.media.session.PlaybackState jlong bitset.
jlong MediaSessionActionsToPlaybackStateActions(
    const MediaSessionClient::AvailableActionsSet& actions) {
  jlong result = 0;
  if (actions[kMediaSessionActionPause]) {
    result |= kPlaybackStateActionPause;
  }
  if (actions[kMediaSessionActionPlay]) {
    result |= kPlaybackStateActionPlay;
  }
  if (actions[kMediaSessionActionSeekbackward]) {
    result |= kPlaybackStateActionRewind;
  }
  if (actions[kMediaSessionActionPrevioustrack]) {
    result |= kPlaybackStateActionSkipToPrevious;
  }
  if (actions[kMediaSessionActionNexttrack]) {
    result |= kPlaybackStateActionSkipToNext;
  }
  if (actions[kMediaSessionActionSeekforward]) {
    result |= kPlaybackStateActionFastForward;
  }
  return result;
}

PlaybackState MediaSessionPlaybackStateToPlaybackState(
    MediaSessionPlaybackState in_state) {
  switch (in_state) {
    case kMediaSessionPlaybackStatePlaying:
      return kPlaying;
    case kMediaSessionPlaybackStatePaused:
      return kPaused;
    case kMediaSessionPlaybackStateNone:
      return kNone;
  }
}

MediaSessionAction PlaybackStateActionToMediaSessionAction(jlong action) {
  MediaSessionAction result;
  switch (action) {
    case kPlaybackStateActionPause:
      result = kMediaSessionActionPause;
      break;
    case kPlaybackStateActionPlay:
      result = kMediaSessionActionPlay;
      break;
    case kPlaybackStateActionRewind:
      result = kMediaSessionActionSeekbackward;
      break;
    case kPlaybackStateActionSkipToPrevious:
      result = kMediaSessionActionPrevioustrack;
      break;
    case kPlaybackStateActionSkipToNext:
      result = kMediaSessionActionNexttrack;
      break;
    case kPlaybackStateActionFastForward:
      result = kMediaSessionActionSeekforward;
      break;
  }
  return result;
}

MediaSessionPlaybackState PlaybackStateToMediaSessionPlaybackState(
    PlaybackState state) {
  MediaSessionPlaybackState result;
  switch (state) {
    case kPlaying:
      result = kMediaSessionPlaybackStatePlaying;
      break;
    case kPaused:
      result = kMediaSessionPlaybackStatePaused;
      break;
    case kNone:
      result = kMediaSessionPlaybackStateNone;
      break;
    default:
      SB_NOTREACHED();
  }
  return result;
}

}  // namespace

class AndroidMediaSessionClient : public MediaSessionClient {
  static SbOnceControl once_flag;
  static SbMutex mutex;
  // The last MediaSessionClient to become active, or null.
  // Used to route Java callbacks.
  // In practice, only one MediaSessionClient will become active at a time.
  // Protected by "mutex"
  static AndroidMediaSessionClient* active_client;

  static void OnceInit() { SbMutexCreate(&mutex); }

 public:
  static void NativeInvokeAction(jlong action) {
    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);

    if (active_client != NULL) {
      MediaSessionAction cobalt_action =
          PlaybackStateActionToMediaSessionAction(action);
      active_client->InvokeAction(cobalt_action);
    }

    SbMutexRelease(&mutex);
  }

  static void UpdateActiveSessionPlatformPlaybackState(
      MediaSessionPlaybackState state) {
    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);

    if (active_client != NULL) {
      active_client->UpdatePlatformPlaybackState(state);
    }

    SbMutexRelease(&mutex);
  }

  AndroidMediaSessionClient() {}

  virtual ~AndroidMediaSessionClient() {
    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);
    if (active_client == this) {
      active_client = NULL;
    }
    SbMutexRelease(&mutex);
  }

  virtual void OnMediaSessionChanged() OVERRIDE {
    JniEnvExt* env = JniEnvExt::Get();

    jint playback_state =
        MediaSessionPlaybackStateToPlaybackState(GetActualPlaybackState());

    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);
    if (playback_state != kNone) {
      active_client = this;
    } else if (active_client == this) {
      active_client = NULL;
    }
    SbMutexRelease(&mutex);

    scoped_refptr<MediaSession> media_session(GetMediaSession());
    scoped_refptr<MediaMetadata> media_metadata(media_session->metadata());

    ScopedLocalJavaRef<jstring> title;
    ScopedLocalJavaRef<jstring> artist;
    ScopedLocalJavaRef<jstring> album;

    if (media_metadata) {
      title.Reset(env->NewStringUTF(media_metadata->title().c_str()));
      artist.Reset(env->NewStringUTF(media_metadata->artist().c_str()));
      album.Reset(env->NewStringUTF(media_metadata->album().c_str()));
    }

    jlong playback_state_actions =
        MediaSessionActionsToPlaybackStateActions(GetAvailableActions());

    env->CallActivityVoidMethodOrAbort(
        "updateMediaSession",
        "(ILjava/lang/String;Ljava/lang/String;Ljava/lang/String;J)V",
        playback_state, title.Get(), artist.Get(), album.Get(),
        playback_state_actions);
  }
};

SbOnceControl AndroidMediaSessionClient::once_flag = SB_ONCE_INITIALIZER;
SbMutex AndroidMediaSessionClient::mutex;
AndroidMediaSessionClient* AndroidMediaSessionClient::active_client = NULL;

void UpdateActiveSessionPlatformPlaybackState(PlaybackState state) {
  MediaSessionPlaybackState media_session_state =
      PlaybackStateToMediaSessionPlaybackState(state);

  AndroidMediaSessionClient::UpdateActiveSessionPlatformPlaybackState(
      media_session_state);
}

}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

using starboard::android::shared::cobalt::AndroidMediaSessionClient;

extern "C" SB_EXPORT_PLATFORM void Java_foo_cobalt_media_nativeInvokeAction(
    JNIEnv* env,
    jlong action) {
  AndroidMediaSessionClient::NativeInvokeAction(action);
}

namespace cobalt {
namespace media_session {

// static
scoped_ptr<MediaSessionClient> MediaSessionClient::Create() {
  return make_scoped_ptr<MediaSessionClient>(new AndroidMediaSessionClient());
}

}  // namespace media_session
}  // namespace cobalt
