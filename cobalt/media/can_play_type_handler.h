// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEDIA_CAN_PLAY_TYPE_HANDLER_H_
#define COBALT_MEDIA_CAN_PLAY_TYPE_HANDLER_H_

#include <string>

#include "starboard/media.h"

namespace cobalt {
namespace media {

class CanPlayTypeHandler {
 public:
  virtual ~CanPlayTypeHandler() {}
  virtual SbMediaSupportType CanPlayProgressive(
      const std::string& mime_type) const = 0;
  virtual SbMediaSupportType CanPlayAdaptive(
      const std::string& mime_type, const std::string& key_system) const = 0;
  virtual void SetDisabledMediaCodecs(const std::string& codecs) = 0;

 protected:
  CanPlayTypeHandler() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CanPlayTypeHandler);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAN_PLAY_TYPE_HANDLER_H_
