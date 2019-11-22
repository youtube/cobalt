// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/loader_app/system_get_extension_shim.h"

#include <string>

#include "cobalt/extension/installation_manager.h"
#include "starboard/common/log.h"
#include "starboard/loader_app/installation_manager.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace {
const CobaltExtensionInstallationManagerApi kInstallationManagerApi = {
    kCobaltExtensionInstallationManagerName,
    1,  // API version that's implemented.
    &ImGetCurrentInstallationIndex,
    &ImMarkInstallationSuccessful,
    &ImRequestRollForwardToInstallation,
    &ImGetInstallationPath,
    &ImSelectNewInstallationIndex,
};
}  // namespace
namespace starboard {
namespace loader_app {

const void* SbSystemGetExtensionShim(const char* name) {
  if (SbStringCompareAll(name, kCobaltExtensionInstallationManagerName) == 0) {
    return &kInstallationManagerApi;
  }
#if SB_API_VERSION >= 11
  return SbSystemGetExtension(name);
#else
  SB_NOTREACHED() << "SbSystemGetExtension is not supported";
  return NULL;
#endif
}
}  // namespace loader_app
}  // namespace starboard
