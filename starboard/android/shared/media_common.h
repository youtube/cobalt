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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_

#include <deque>
#include <queue>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/mutex.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"
#include "starboard/string.h"

namespace starboard {
namespace android {
namespace shared {

const int64_t kSecondInMicroseconds = 1000 * 1000;

inline bool IsWidevine(const char* key_system) {
  return SbStringCompareAll(key_system, "com.widevine") == 0 ||
         SbStringCompareAll(key_system, "com.widevine.alpha") == 0;
}

// Map a supported |SbMediaAudioCodec| into its corresponding mime type
// string.  Returns |NULL| if |audio_codec| is not supported.
inline const char* SupportedAudioCodecToMimeType(
    const SbMediaAudioCodec audio_codec) {
  if (audio_codec == kSbMediaAudioCodecAac) {
    return "audio/mp4a-latm";
  }
  return NULL;
}

// Map a supported |SbMediaVideoCodec| into its corresponding mime type
// string.  Returns |NULL| if |video_codec| is not supported.
inline const char* SupportedVideoCodecToMimeType(
    const SbMediaVideoCodec video_codec) {
  if (video_codec == kSbMediaVideoCodecVp9) {
    return "video/x-vnd.on2.vp9";
  } else if (video_codec == kSbMediaVideoCodecH264) {
    return "video/avc";
  }
  return NULL;
}

// A simple thread-safe queue for events of type |E|, that supports polling
// based access only.
template <typename E>
class EventQueue {
 public:
  E PollFront() {
    ScopedLock lock(mutex_);
    if (!deque_.empty()) {
      E event = deque_.front();
      deque_.pop_front();
      return event;
    }

    return E();
  }

  void PushBack(const E& event) {
    ScopedLock lock(mutex_);
    deque_.push_back(event);
  }

  void Clear() {
    ScopedLock lock(mutex_);
    deque_.clear();
  }

  size_t size() const {
    ScopedLock lock(mutex_);
    return deque_.size();
  }

 private:
  ::starboard::Mutex mutex_;
  std::deque<E> deque_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
