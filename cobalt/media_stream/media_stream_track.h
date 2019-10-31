// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_TRACK_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_TRACK_H_

#include "base/callback.h"
#include "base/strings/string_piece.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_stream/media_track_settings.h"
#include "cobalt/script/environment_settings.h"
#include "starboard/common/mutex.h"

namespace cobalt {
namespace media_stream {

// This class represents a MediaStreamTrack, and implements the specification
// at: https://www.w3.org/TR/mediacapture-streams/#dom-mediastreamtrack
class MediaStreamTrack : public dom::EventTarget {
 public:
  enum ReadyState {
    kReadyStateLive,
    kReadyStateEnded,
  };

  explicit MediaStreamTrack(script::EnvironmentSettings* settings)
      : EventTarget(settings) {}

  // Function exposed to JavaScript via IDL.
  const MediaTrackSettings& GetSettings() const {
    starboard::ScopedLock lock(settings_mutex_);
    return settings_;
  }

  const MediaTrackSettings& GetMediaTrackSettings() const {
    return GetSettings();
  }

  virtual void Stop() = 0;
  virtual void OnReadyStateChanged(
      media_stream::MediaStreamTrack::ReadyState new_state) = 0;

  void SetMediaTrackSettings(const MediaTrackSettings& new_settings) {
    starboard::ScopedLock lock(settings_mutex_);
    settings_ = new_settings;
  }

  DEFINE_WRAPPABLE_TYPE(MediaStreamTrack);

 private:
  MediaStreamTrack(const MediaStreamTrack&) = delete;
  MediaStreamTrack& operator=(const MediaStreamTrack&) = delete;

  starboard::Mutex settings_mutex_;
  MediaTrackSettings settings_;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_TRACK_H_
