// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/examples/utils.h"

#include "starboard/common/log.h"
#include "starboard/extension/time_zone.h"
#include "starboard/file.h"
#include "starboard/log.h"
#include "starboard/system.h"

bool StateChangingFunction(const char* path) {
  SbLogFormatF("Going to delete: %s", path);
  return SbFileDelete(path);
}

bool StateChangingFunctionWithSbExtensionDep(const char* time_zone_name) {
  auto time_zone_extension = static_cast<const StarboardExtensionTimeZoneApi*>(
      SbSystemGetExtension(kStarboardExtensionTimeZoneName));
  if (time_zone_extension && time_zone_extension->version >= 1) {
    // This log statement causes the unit test to fail because
    // SbSystemGetExtension, which the test stubs out, is used in the ICU
    // library. https://paste.googleplex.com/6243528758263808 has the call stack
    // and steps to reproduce with gdb.
    // SB_LOG(INFO) << "Extension found, going to set the time zone";
    return time_zone_extension->SetTimeZone(time_zone_name);
  }
  return false;
}
