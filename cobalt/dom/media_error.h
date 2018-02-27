// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_DOM_MEDIA_ERROR_H_
#define COBALT_DOM_MEDIA_ERROR_H_

#include <string>

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The MediaError represents a media element error with an error code.
//   https://www.w3.org/TR/html5/embedded-content-0.html#mediaerror
// kMediaErrEncrypted(MEDIA_ERR_ENCRYPTED) is introduced in EME 0.1b.
//   https://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#dom-mediakeyerror
class MediaError : public script::Wrappable {
 public:
  // Web API: MediaError
  //
  enum Code {
    kMediaErrAborted = 1,
    kMediaErrNetwork = 2,
    kMediaErrDecode = 3,
    kMediaErrSrcNotSupported = 4,
    kMediaErrEncrypted = 5,
  };

  // Custom, not in any spec.
  //
  explicit MediaError(Code code, const std::string& message = "")
      : code_(code), message_(message) {}

  // Web API: MediaError
  //
  uint32 code() const { return code_; }
  const std::string& message() const { return message_; }

  DEFINE_WRAPPABLE_TYPE(MediaError);

 private:
  Code code_;
  std::string message_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_MEDIA_ERROR_H_
