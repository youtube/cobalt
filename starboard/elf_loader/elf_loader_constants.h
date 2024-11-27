// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ELF_LOADER_ELF_LOADER_CONSTANTS_H_
#define STARBOARD_ELF_LOADER_ELF_LOADER_CONSTANTS_H_

#include "starboard/configuration.h"

namespace starboard {
namespace elf_loader {

// File path suffix for compressed binaries.
extern const char kCompressionSuffix[];

// Command line flag to specify the library path.
extern const char kEvergreenLibrary[];

// Command line flag to specify the content path.
extern const char kEvergreenContent[];

}  // namespace elf_loader
}  // namespace starboard

#endif  // STARBOARD_ELF_LOADER_ELF_LOADER_CONSTANTS_H_
