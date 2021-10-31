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

#ifndef COBALT_DOM_VIDEO_TRACK_LIST_H_
#define COBALT_DOM_VIDEO_TRACK_LIST_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/track_list_base.h"
#include "cobalt/dom/video_track.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The VideoTrackList interface holds a list of video tracks
//   https://www.w3.org/TR/html51/semantics-embedded-content.html#videotracklist-videotracklist
class VideoTrackList : public TrackListBase<VideoTrack> {
 public:
  // Custom, not in any spec.
  //
  VideoTrackList(script::EnvironmentSettings* settings,
                 HTMLMediaElement* media_element)
      : TrackListBase<VideoTrack>(settings, media_element) {}

  // Web API: VideoTrackList
  //
  using TrackListBase<VideoTrack>::length;
  using TrackListBase<VideoTrack>::AnonymousIndexedGetter;
  using TrackListBase<VideoTrack>::GetTrackById;

  int32 selected_index() const {
    for (uint32 i = 0; i < length(); ++i) {
      scoped_refptr<VideoTrack> video_track = AnonymousIndexedGetter(i);
      if (video_track->selected()) {
        return static_cast<int32>(i);
      }
    }

    return -1;
  }

  // Custom, not in any spec.
  //
  // Called when the VideoTrack identified by |id| is selected.  This function
  // clears the selected flags of other VideoTracks if there is any.
  void OnTrackSelected(const std::string& id) {
    for (uint32 i = 0; i < length(); ++i) {
      scoped_refptr<VideoTrack> video_track = AnonymousIndexedGetter(i);

      if (video_track->id() != id) {
        video_track->deselect();
      } else {
        DCHECK(video_track->selected());
      }
    }
  }

  DEFINE_WRAPPABLE_TYPE(VideoTrackList);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_VIDEO_TRACK_LIST_H_
