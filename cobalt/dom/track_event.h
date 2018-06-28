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

#ifndef COBALT_DOM_TRACK_EVENT_H_
#define COBALT_DOM_TRACK_EVENT_H_

#include <string>

#include "cobalt/base/token.h"
#include "cobalt/dom/audio_track.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/video_track.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The TrackEvent provides specific contextual information associated with
// events fired on AudioTrack, VideoTrack, and TextTrack objects.
//   https://www.w3.org/TR/html51/semantics-embedded-content.html#trackevent-trackevent
class TrackEvent : public Event {
 public:
  // Custom, not in any spec.
  //
  explicit TrackEvent(const std::string& type) : Event(base::Token(type)) {}
  TrackEvent(base::Token type, const scoped_refptr<AudioTrack>& audio_track)
      : Event(type), track_(audio_track) {}
  TrackEvent(base::Token type, const scoped_refptr<VideoTrack>& video_track)
      : Event(type), track_(video_track) {}

  // Web API: TrackEvent
  //
  scoped_refptr<script::Wrappable> track() const { return track_; }

  DEFINE_WRAPPABLE_TYPE(TrackEvent);

 private:
  scoped_refptr<script::Wrappable> track_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRACK_EVENT_H_
