// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/system.h"

#include "starboard/common/string.h"
#include "starboard/extension/configuration.h"
#include "starboard/extension/graphics.h"
#include "starboard/extension/media_session.h"
#include "starboard/shared/uwp/xb1_media_session_client.h"
#include "starboard/shared/win32/graphics.h"
#include "starboard/xb1/shared/configuration.h"

const void* SbSystemGetExtension(const char* name) {
  if (strcmp(name, kCobaltExtensionGraphicsName) == 0) {
    return starboard::shared::win32::GetGraphicsApi();
  }
  if (strcmp(name, kCobaltExtensionConfigurationName) == 0) {
    return starboard::xb1::shared::GetConfigurationApi();
  }
  if (strcmp(name, kCobaltExtensionMediaSessionName) == 0) {
    return starboard::shared::uwp::GetMediaSessionApi();
  }
  return NULL;
}
