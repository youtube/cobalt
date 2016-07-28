/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_DOM_MEDIA_KEY_ERROR_H_
#define COBALT_DOM_MEDIA_KEY_ERROR_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The MediaKeyError represents an error in Encrypted Media Extension.
//   https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-mediakeyerror
class MediaKeyError : public script::Wrappable {
 public:
  // Web API: MediaKeyError
  //
  enum Code {
    kMediaKeyerrUnknown = 1,
    kMediaKeyerrClient = 2,
    kMediaKeyerrService = 3,
    kMediaKeyerrOutput = 4,
    kMediaKeyerrHardwarechange = 5,
    kMediaKeyerrDomain = 6,
  };

  DEFINE_WRAPPABLE_TYPE(MediaKeyError);
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_KEY_ERROR_H_
