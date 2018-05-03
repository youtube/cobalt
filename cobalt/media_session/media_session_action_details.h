// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_SESSION_MEDIA_SESSION_ACTION_DETAILS_H_
#define COBALT_MEDIA_SESSION_MEDIA_SESSION_ACTION_DETAILS_H_

#include "base/logging.h"
#include "cobalt/media_session/media_session_action.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_session {

class MediaSessionActionDetails : public script::Wrappable {
 public:
  // Non-Wrappable class holding the data needed for selected actions. This can
  // be constructed on any thread, and then it's used internally to construct
  // MediaSessionActionDetails on the web module thread.
  class Data {
   public:
    explicit Data(MediaSessionAction action, double seek = 0.0)
        : action_(action),
          seek_time_(action == kMediaSessionActionSeek ? seek : 0.0),
          seek_offset_(
              (action == kMediaSessionActionSeekforward
                  || action == kMediaSessionActionSeekbackward) ? seek : 0.0) {
      DCHECK(seek >= 0.0);
      DCHECK(seek == 0.0
             || action == kMediaSessionActionSeek
             || action == kMediaSessionActionSeekforward
             || action == kMediaSessionActionSeekbackward);
    }
    Data(const Data&) = default;

    MediaSessionAction action() const { return action_; }

   private:
    friend class MediaSessionActionDetails;

    MediaSessionAction action_;
    double seek_time_;
    double seek_offset_;
  };

  explicit MediaSessionActionDetails(const Data& data) : data_(data) {}

  MediaSessionAction action() const { return data_.action_; }
  double seek_time() const { return data_.seek_time_; }
  double seek_offset() const { return data_.seek_offset_; }

  DEFINE_WRAPPABLE_TYPE(MediaSessionActionDetails);

 private:
  Data data_;
};

}  // namespace media_session
}  // namespace cobalt

#endif  // COBALT_MEDIA_SESSION_MEDIA_SESSION_ACTION_DETAILS_H_
