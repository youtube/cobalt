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

#include "starboard/android/shared/cobalt/android_media_session_client.h"

#include "base/time/time.h"
#include "cobalt/media_session/media_session_action_details.h"
#include "cobalt/media_session/media_session_client.h"
#include "cobalt/script/sequence.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/once.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

using ::cobalt::media_session::MediaImage;
using ::cobalt::media_session::MediaMetadataInit;
using ::cobalt::media_session::MediaSession;
using ::cobalt::media_session::MediaSessionAction;
using ::cobalt::media_session::MediaSessionActionDetails;
using ::cobalt::media_session::MediaSessionClient;
using ::cobalt::media_session::MediaSessionPlaybackState;
using ::cobalt::media_session::MediaSessionState;
using ::cobalt::media_session::kMediaSessionActionPause;
using ::cobalt::media_session::kMediaSessionActionPlay;
using ::cobalt::media_session::kMediaSessionActionSeekto;
using ::cobalt::media_session::kMediaSessionActionSeekbackward;
using ::cobalt::media_session::kMediaSessionActionSeekforward;
using ::cobalt::media_session::kMediaSessionActionStop;
using ::cobalt::media_session::kMediaSessionActionPrevioustrack;
using ::cobalt::media_session::kMediaSessionActionNexttrack;
using ::cobalt::media_session::kMediaSessionPlaybackStateNone;
using ::cobalt::media_session::kMediaSessionPlaybackStatePaused;
using ::cobalt::media_session::kMediaSessionPlaybackStatePlaying;

using MediaImageSequence = ::cobalt::script::Sequence<MediaImage>;

using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::ScopedLocalJavaRef;

namespace {

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
jlong MediaSessionActionsToPlaybackStateActions(
    const MediaSessionState::AvailableActionsSet& actions) {
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
  if (actions[kMediaSessionActionSeekto]) {
    result |= kPlaybackStateActionSeekTo;
  }
  if (actions[kMediaSessionActionStop]) {
    result |= kPlaybackStateActionStop;
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
    case kPlaybackStateActionSeekTo:
      result = kMediaSessionActionSeekto;
      break;
    case kPlaybackStateActionStop:
      result = kMediaSessionActionStop;
      break;
    default:
      SB_NOTREACHED() << "Unsupported MediaSessionAction 0x"
                      << std::hex << action;
      result = static_cast<MediaSessionAction>(-1);
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
      SB_NOTREACHED() << "Unsupported PlaybackState " << state;
      result = static_cast<MediaSessionPlaybackState>(-1);
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
  static void NativeInvokeAction(jlong action, jlong seek_ms) {
    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);

    if (active_client != NULL) {
      std::unique_ptr<MediaSessionActionDetails> details(
          new MediaSessionActionDetails());
      details->set_action(PlaybackStateActionToMediaSessionAction(action));
      // CobaltMediaSession.java only sets seek_ms for SeekTo (not ff/rew).
      if (details->action() == kMediaSessionActionSeekto) {
        details->set_seek_time(seek_ms / 1000.0);
      }
      active_client->InvokeAction(std::move(details));
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

  void OnMediaSessionStateChanged(
      const MediaSessionState& session_state) override {
    JniEnvExt* env = JniEnvExt::Get();

    jint playback_state = MediaSessionPlaybackStateToPlaybackState(
        session_state.actual_playback_state());

    SbOnce(&once_flag, OnceInit);
    SbMutexAcquire(&mutex);
    if (playback_state != kNone) {
      active_client = this;
    } else if (active_client == this) {
      active_client = NULL;
    }
    SbMutexRelease(&mutex);

    jlong playback_state_actions = MediaSessionActionsToPlaybackStateActions(
        session_state.available_actions());

    ScopedLocalJavaRef<jstring> j_title;
    ScopedLocalJavaRef<jstring> j_artist;
    ScopedLocalJavaRef<jstring> j_album;
    ScopedLocalJavaRef<jobjectArray> j_artwork;

    if (session_state.has_metadata()) {
      const MediaMetadataInit& media_metadata(session_state.metadata().value());

      j_title.Reset(
          env->NewStringStandardUTFOrAbort(media_metadata.title().c_str()));
      j_artist.Reset(
          env->NewStringStandardUTFOrAbort(media_metadata.artist().c_str()));
      j_album.Reset(
          env->NewStringStandardUTFOrAbort(media_metadata.album().c_str()));

      if (media_metadata.has_artwork()) {
        const MediaImageSequence& artwork(media_metadata.artwork());
        ScopedLocalJavaRef<jclass> media_image_class(
            env->FindClassExtOrAbort("dev/cobalt/media/MediaImage"));
        jmethodID media_image_constructor = env->GetMethodID(
            media_image_class.Get(), "<init>",
            "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
        env->AbortOnException();

        j_artwork.Reset(static_cast<jobjectArray>(env->NewObjectArray(
            artwork.size(), media_image_class.Get(), NULL)));
        env->AbortOnException();

        ScopedLocalJavaRef<jstring> j_src;
        ScopedLocalJavaRef<jstring> j_sizes;
        ScopedLocalJavaRef<jstring> j_type;
        for (MediaImageSequence::size_type i = 0; i < artwork.size(); i++) {
          const MediaImage& media_image(artwork.at(i));
          j_src.Reset(
              env->NewStringStandardUTFOrAbort(media_image.src().c_str()));
          j_sizes.Reset(
              env->NewStringStandardUTFOrAbort(media_image.sizes().c_str()));
          j_type.Reset(
              env->NewStringStandardUTFOrAbort(media_image.type().c_str()));

          ScopedLocalJavaRef<jobject> j_media_image(
              env->NewObject(media_image_class.Get(), media_image_constructor,
                             j_src.Get(), j_sizes.Get(), j_type.Get()));

          env->SetObjectArrayElement(j_artwork.Get(), i, j_media_image.Get());
        }
      }
    }

    env->CallStarboardVoidMethodOrAbort(
        "updateMediaSession",
        "(IJJFLjava/lang/String;Ljava/lang/String;Ljava/lang/String;"
            "[Ldev/cobalt/media/MediaImage;)V",
        playback_state, playback_state_actions,
        session_state.current_playback_position() / kSbTimeMillisecond,
        static_cast<jfloat>(session_state.actual_playback_rate()),
        j_title.Get(), j_artist.Get(), j_album.Get(), j_artwork.Get());
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

extern "C" SB_EXPORT_PLATFORM
void Java_dev_cobalt_media_CobaltMediaSession_nativeInvokeAction(
    JNIEnv* env,
    jclass unused_clazz,
    jlong action,
    jlong seek_ms) {
  AndroidMediaSessionClient::NativeInvokeAction(action, seek_ms);
}

namespace cobalt {
namespace media_session {

// static
std::unique_ptr<MediaSessionClient> MediaSessionClient::Create() {
  return std::unique_ptr<MediaSessionClient>(new AndroidMediaSessionClient());
}

}  // namespace media_session
}  // namespace cobalt
