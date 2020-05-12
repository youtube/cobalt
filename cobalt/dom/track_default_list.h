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

#ifndef COBALT_DOM_TRACK_DEFAULT_LIST_H_
#define COBALT_DOM_TRACK_DEFAULT_LIST_H_

#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/dom/track_default.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The TrackDefaultList is a simple container object for TrackDefault objects.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#trackdefaultlist
class TrackDefaultList : public script::Wrappable {
 public:
  // Web API: TrackDefaultList
  //
  explicit TrackDefaultList(script::ExceptionState* exception_state) {
  }
  TrackDefaultList(
      const script::Sequence<scoped_refptr<TrackDefault> >& track_defaults,
      script::ExceptionState* exception_state) {
    // |track_defaults| shouldn't contain two or more TrackDefault objects with
    // the same |type| and the same |byte_stream_track_id|.
    typedef std::pair<TrackDefaultType, std::string> TypeAndId;
    std::set<TypeAndId> type_and_ids;
    for (script::Sequence<scoped_refptr<TrackDefault> >::size_type i = 0;
         i < track_defaults.size(); ++i) {
      scoped_refptr<TrackDefault> track_default = track_defaults.at(i);
      TypeAndId type_and_id(track_default->type(),
                            track_default->byte_stream_track_id());
      if (!type_and_ids.insert(type_and_id).second) {
        DOMException::Raise(DOMException::kInvalidAccessErr, exception_state);
        return;
      }
    }
    track_defaults_ = track_defaults;
  }

  uint32 length() const { return static_cast<uint32>(track_defaults_.size()); }
  scoped_refptr<TrackDefault> AnonymousIndexedGetter(uint32 index) const {
    if (index < length()) {
      return track_defaults_.at(index);
    }
    return NULL;
  }

  DEFINE_WRAPPABLE_TYPE(TrackDefaultList);

 private:
  script::Sequence<scoped_refptr<TrackDefault> > track_defaults_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRACK_DEFAULT_LIST_H_
