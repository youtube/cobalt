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

#ifndef COBALT_DOM_TRACK_LIST_BASE_H_
#define COBALT_DOM_TRACK_LIST_BASE_H_

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "cobalt/base/tokens.h"
#include "cobalt/dom/event.h"
#include "cobalt/dom/event_target.h"
#include "cobalt/dom/html_media_element.h"
#include "cobalt/dom/track_event.h"
#include "cobalt/script/environment_settings.h"

namespace cobalt {
namespace dom {

// This class contains common operations for AudioTrackList and VideoTrackList.
template <typename TrackType>
class TrackListBase : public EventTarget {
 public:
  TrackListBase(script::EnvironmentSettings* settings,
                HTMLMediaElement* media_element)
      : EventTarget(settings) {
    DCHECK(media_element);
    media_element_ = base::AsWeakPtr(media_element);
  }

  uint32 length() const { return static_cast<uint32>(tracks_.size()); }
  scoped_refptr<TrackType> AnonymousIndexedGetter(uint32 index) const {
    if (index >= tracks_.size()) return NULL;
    return tracks_[index];
  }

  scoped_refptr<TrackType> GetTrackById(const std::string& id) const {
    for (size_t i = 0; i < tracks_.size(); ++i) {
      if (tracks_[i]->id() == id) {
        return tracks_[i];
      }
    }

    return NULL;
  }

  void Add(const scoped_refptr<TrackType>& track) {
    track->SetMediaElement(media_element_);
    tracks_.push_back(track);
    ScheduleEvent(new TrackEvent(base::Tokens::addtrack(), track));
  }

  void Remove(const std::string& id) {
    for (size_t i = 0; i < tracks_.size(); ++i) {
      scoped_refptr<TrackType> track = tracks_[i];
      if (track->id() != id) {
        continue;
      }
      track->SetMediaElement(NULL);
      ScheduleEvent(new TrackEvent(base::Tokens::removetrack(), track));
      tracks_.erase(tracks_.begin() + i);
      return;
    }
    NOTREACHED();
  }

  void RemoveAll() {
    for (size_t i = 0; i < tracks_.size(); ++i) {
      tracks_[i]->SetMediaElement(NULL);
    }

    tracks_.clear();
  }

  void ScheduleChangeEvent() {
    ScheduleEvent(new Event(base::Tokens::change()));
  }

 private:
  void ScheduleEvent(const scoped_refptr<Event>& event) {
    event->set_target(this);
    media_element_->ScheduleEvent(event);
  }

  base::WeakPtr<HTMLMediaElement> media_element_;
  std::vector<scoped_refptr<TrackType> > tracks_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRACK_LIST_BASE_H_
