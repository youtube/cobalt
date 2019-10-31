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

#ifndef COBALT_DOM_AUDIO_TRACK_LIST_H_
#define COBALT_DOM_AUDIO_TRACK_LIST_H_

#include "cobalt/dom/audio_track.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/track_list_base.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The AudioTrackList interface holds a list of audio tracks
//   https://www.w3.org/TR/html51/semantics-embedded-content.html#audiotracklist-audiotracklist
class AudioTrackList : public TrackListBase<AudioTrack> {
 public:
  // Custom, not in any spec.
  //
  AudioTrackList(script::EnvironmentSettings* settings,
                 HTMLMediaElement* media_element)
      : TrackListBase<AudioTrack>(settings, media_element) {}

  // Web API: AudioTrackList
  //
  using TrackListBase<AudioTrack>::length;
  using TrackListBase<AudioTrack>::AnonymousIndexedGetter;
  using TrackListBase<AudioTrack>::GetTrackById;

  // Custom, not in any spec.
  //
  bool HasEnabledTrack() const {
    for (uint32 i = 0; i < length(); ++i) {
      if (AnonymousIndexedGetter(i)->enabled()) {
        return true;
      }
    }

    return false;
  }

  DEFINE_WRAPPABLE_TYPE(AudioTrackList);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_AUDIO_TRACK_LIST_H_
