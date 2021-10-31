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

#include "starboard/image.h"

#include "starboard/raspi/shared/open_max/open_max_image_decode_component.h"
#include "starboard/shared/starboard/application.h"

namespace open_max = starboard::raspi::shared::open_max;

bool SbImageIsDecodeSupported(const char* mime_type,
                              SbDecodeTargetFormat format) {
  using starboard::shared::starboard::Application;

  auto command_line = Application::Get()->GetCommandLine();
  auto value = command_line->GetSwitchValue("enable_sb_image_decoder");
  // Unfortunately, while openmax image decoding is implemented and supported,
  // there is a very sporadic (i.e. around every 1 in 200 image decodes) crash
  // bug that will go off.  This may be due to a threading issue somewhere, but
  // it is not clear right now.  So by default we report that we do not support
  // Starboard image decoding.
  if (value != "true") {
    return false;
  }

  bool type_supported =
      OMX_IMAGE_CodingMax !=
      open_max::OpenMaxImageDecodeComponent::GetCompressionFormat(mime_type);
  return type_supported && format == kSbDecodeTargetFormat1PlaneRGBA;
}
