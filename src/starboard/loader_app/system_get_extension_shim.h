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

// ELF Loader shim functions are intended to be patched into the Cobalt shared
// library when the library is loaded. Cobalt will then invoke the shim function
// where it would otherwise invoke the original function, allowing us to
// partially or entirely change the behavior within that function call.

#ifndef STARBOARD_LOADER_APP_SYSTEM_GET_EXTENSION_SHIM_H_
#define STARBOARD_LOADER_APP_SYSTEM_GET_EXTENSION_SHIM_H_

#include "starboard/system.h"

namespace starboard {
namespace loader_app {

// Adds the |kCobaltExtensionInstallationManagerName| to the list of extensions
// supported by SbSystemGetExtension() by wrapping the call to
// SbSystemGetExtension.
//
// Please reference //starboard/system.h for documentation on
// SbSystemGetExtension().
const void* SbSystemGetExtensionShim(const char* name);

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_SYSTEM_GET_EXTENSION_SHIM_H_
