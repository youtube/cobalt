// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/image.h"

#include "starboard/common/string.h"
#include "starboard/decode_target.h"
#include "starboard/shared/starboard/application.h"

bool SbImageIsDecodeSupported(const char* mime_type,
                              SbDecodeTargetFormat format) {
  using starboard::shared::starboard::Application;

  auto command_line = Application::Get()->GetCommandLine();
  auto value = command_line->GetSwitchValue("enable_sb_image_decoder");
  if (value == "true") {
    if (format != kSbDecodeTargetFormat1PlaneRGBA &&
        format != kSbDecodeTargetFormat1PlaneBGRA) {
      return false;
    }
    return strcmp(mime_type, "image/jpeg") == 0;
  } else {
    return false;
  }
}
