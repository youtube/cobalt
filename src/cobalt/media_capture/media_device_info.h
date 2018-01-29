// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_CAPTURE_MEDIA_DEVICE_INFO_H_
#define COBALT_MEDIA_CAPTURE_MEDIA_DEVICE_INFO_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cobalt/media_capture/media_device_kind.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/script/sequence.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace media_capture {

class MediaDeviceInfo : public script::Wrappable {
 public:
  MediaDeviceInfo(script::ScriptValueFactory* script_value_factory,
                  MediaDeviceKind kind,
                  const std::string& label);

  MediaDeviceKind kind() { return kind_; }
  const std::string label() const { return label_; }

  DEFINE_WRAPPABLE_TYPE(MediaDeviceInfo);

 private:
  script::ScriptValueFactory* script_value_factory_;
  MediaDeviceKind kind_;
  std::string label_;

  DISALLOW_COPY_AND_ASSIGN(MediaDeviceInfo);
};

}  // namespace media_capture
}  // namespace cobalt

#endif  // COBALT_MEDIA_CAPTURE_MEDIA_DEVICE_INFO_H_
