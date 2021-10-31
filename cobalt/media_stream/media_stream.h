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

#ifndef COBALT_MEDIA_STREAM_MEDIA_STREAM_H_
#define COBALT_MEDIA_STREAM_MEDIA_STREAM_H_

#include "base/memory/ref_counted.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/media_stream/media_stream_track.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_stream {

// This class represents a MediaStream, and implements the specification at:
// https://www.w3.org/TR/mediacapture-streams/#dom-mediastream
class MediaStream : public dom::EventTarget {
 public:
  using TrackSequences = script::Sequence<scoped_refptr<MediaStreamTrack>>;

  // Constructors.
  explicit MediaStream(script::EnvironmentSettings* settings)
      : dom::EventTarget(settings) {}

  MediaStream(script::EnvironmentSettings* settings, TrackSequences tracks)
      : dom::EventTarget(settings), tracks_(std::move(tracks)) {}

  // Functions.
  script::Sequence<scoped_refptr<MediaStreamTrack>>& GetAudioTracks() {
    return tracks_;
  }

  script::Sequence<scoped_refptr<MediaStreamTrack>>& GetTracks() {
    return GetAudioTracks();
  }

  void TraceMembers(script::Tracer* tracer) override {
    EventTarget::TraceMembers(tracer);
    tracer->TraceItems(tracks_);
  }

  DEFINE_WRAPPABLE_TYPE(MediaStream);

 private:
  MediaStream(const MediaStream&) = delete;
  MediaStream& operator=(const MediaStream&) = delete;

  script::Sequence<scoped_refptr<MediaStreamTrack>> tracks_;
};

}  // namespace media_stream
}  // namespace cobalt

#endif  // COBALT_MEDIA_STREAM_MEDIA_STREAM_H_
