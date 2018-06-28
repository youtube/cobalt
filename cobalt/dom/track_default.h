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

#ifndef COBALT_DOM_TRACK_DEFAULT_H_
#define COBALT_DOM_TRACK_DEFAULT_H_

#include <string>

#include "base/logging.h"
#include "cobalt/dom/audio_track.h"
#include "cobalt/dom/track_default_type.h"
#include "cobalt/dom/video_track.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The TrackDefault object is used to provide kind, label, and language
// information for tracks that do not contain this information in the
// initialization segments.
//   https://www.w3.org/TR/2016/CR-media-source-20160705/#trackdefault
class TrackDefault : public script::Wrappable {
 public:
  // Web API: TrackDefault
  //
  TrackDefault(TrackDefaultType type, const std::string& language,
               const std::string& label,
               const script::Sequence<std::string>& kinds,
               const std::string& byte_stream_track_id,
               script::ExceptionState* exception_state)
      : type_(type),
        byte_stream_track_id_(byte_stream_track_id),
        language_(language),
        label_(label),
        kinds_(kinds) {
    if (type == kTrackDefaultTypeAudio) {
      for (script::Sequence<std::string>::size_type i = 0; i < kinds.size();
           ++i) {
        if (!AudioTrack::IsValidKind(kinds.at(i).c_str())) {
          exception_state->SetSimpleException(script::kSimpleTypeError);
        }
      }
    } else if (type == kTrackDefaultTypeVideo) {
      for (script::Sequence<std::string>::size_type i = 0; i < kinds.size();
           ++i) {
        if (!VideoTrack::IsValidKind(kinds.at(i).c_str())) {
          exception_state->SetSimpleException(script::kSimpleTypeError);
        }
      }
    } else {
      NOTREACHED();
    }
  }

  TrackDefaultType type() const { return type_; }
  const std::string& byte_stream_track_id() const {
    return byte_stream_track_id_;
  }
  const std::string& language() const { return language_; }
  const std::string& label() const { return label_; }
  const script::Sequence<std::string>& GetKinds() const { return kinds_; }

  DEFINE_WRAPPABLE_TYPE(TrackDefault);

 private:
  TrackDefaultType type_;
  const std::string byte_stream_track_id_;
  const std::string language_;
  const std::string label_;
  const script::Sequence<std::string> kinds_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_TRACK_DEFAULT_H_
