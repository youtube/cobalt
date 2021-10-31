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

#ifndef COBALT_WEBDRIVER_PROTOCOL_FRAME_ID_H_
#define COBALT_WEBDRIVER_PROTOCOL_FRAME_ID_H_

#include "base/optional.h"
#include "base/values.h"

namespace cobalt {
namespace webdriver {
namespace protocol {

// https://github.com/SeleniumHQ/selenium/wiki/JsonWireProtocol#sessionsessionidframe
// Since Cobalt doesn't support multiple frames, the only valid value for this
// command is to request switching to the top-level browsing context, which
// is always active.
class FrameId {
 public:
  static base::Optional<FrameId> FromValue(const base::Value* value);

  bool is_top_level_browsing_context() const {
    return is_top_level_browsing_context_;
  }

 private:
  explicit FrameId(bool top_level)
      : is_top_level_browsing_context_(top_level) {}
  bool is_top_level_browsing_context_;
};

}  // namespace protocol
}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_PROTOCOL_FRAME_ID_H_
