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

#ifndef COBALT_DOM_VIDEO_TRACK_H_
#define COBALT_DOM_VIDEO_TRACK_H_

#include <string>

#include "cobalt/dom/track_base.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

class SourceBuffer;

// The VideoTrack interface holds attributes of an video track.
//   https://www.w3.org/TR/html51/semantics-embedded-content.html#videotrack-videotrack
class VideoTrack : public TrackBase {
 public:
  // Custom, not in any spec.
  //
  VideoTrack(const std::string& id, const std::string& kind,
             const std::string& label, const std::string& language,
             bool selected)
      : TrackBase(id, kind, label, language, &VideoTrack::IsValidKind),
        selected_(selected) {}

  // Web API: VideoTrack
  //
  using TrackBase::id;
  using TrackBase::kind;
  using TrackBase::label;
  using TrackBase::language;
  bool selected() const { return selected_; }
  void set_selected(bool selected) {
    if (selected_ == selected) {
      return;
    }
    selected_ = selected;
    if (media_element_) {
      // TODO: Notify the media element.
      // media_element_->OnSelectedVideoTrackChanged(this);
      NOTREACHED();
    }
  }

  // Custom, not in any spec.
  //
  static bool IsValidKind(const char* kind) {
    // https://www.w3.org/TR/html51/semantics-embedded-content.html#dom-videotrack-videotrackkind
    return strcmp(kind, "alternative") == 0 ||
           strcmp(kind, "captions") == 0 ||
           strcmp(kind, "main") == 0 ||
           strcmp(kind, "sign") == 0 ||
           strcmp(kind, "subtitles") == 0 ||
           strcmp(kind, "commentary") == 0 ||
           strlen(kind) == 0;
  }

  // Reset selected flag without notifying the media element.  This is used by
  // the VideoTrackList to clear the select flag of other VideoTracks when a
  // VideoTrack is selected.
  void deselect() { selected_ = false; }

  DEFINE_WRAPPABLE_TYPE(VideoTrack);

 private:
  bool selected_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_VIDEO_TRACK_H_
