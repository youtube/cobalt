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

#include "base/time.h"
#include "cobalt/media_session/media_session_client.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/log.h"

namespace starboard {
namespace android {
namespace shared {
namespace cobalt {

using ::cobalt::media_session::MediaMetadata;
using ::cobalt::media_session::MediaSessionClient;
using ::cobalt::media_session::MediaSession;
using ::cobalt::media_session::kMediaSessionPlaybackStateNone;
using ::starboard::android::shared::JniEnvExt;
using ::starboard::android::shared::ScopedLocalJavaRef;

class AndroidMediaSessionClient : public MediaSessionClient {
  virtual void OnMediaSessionChanged() OVERRIDE {
    JniEnvExt* env = JniEnvExt::Get();

    scoped_refptr<MediaSession> media_session(GetMediaSession());
    scoped_refptr<MediaMetadata> media_metadata(media_session->metadata());

    ScopedLocalJavaRef title;
    ScopedLocalJavaRef artist;
    ScopedLocalJavaRef album;

    if (media_metadata) {
      title.Reset(env->NewStringUTF(media_metadata->title().c_str()));
      artist.Reset(env->NewStringUTF(media_metadata->artist().c_str()));
      album.Reset(env->NewStringUTF(media_metadata->album().c_str()));
    }

    jboolean sessionActive = static_cast<jboolean>(
        kMediaSessionPlaybackStateNone == GetActualPlaybackState());

    env->CallActivityVoidMethodOrAbort(
        "updateMediaSession",
        "(ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
        sessionActive, title.Get(), artist.Get(), album.Get());
  }
};
}  // namespace cobalt
}  // namespace shared
}  // namespace android
}  // namespace starboard

using starboard::android::shared::cobalt::AndroidMediaSessionClient;

namespace cobalt {
namespace media_session {

// static
scoped_ptr<MediaSessionClient> MediaSessionClient::Create() {
  return make_scoped_ptr<MediaSessionClient>(new AndroidMediaSessionClient());
}

}  // namespace media_session
}  // namespace cobalt
