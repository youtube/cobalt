// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Module Overview: Starboard Export module
//
// Provides macros for properly exporting or importing symbols from shared
// libraries.
#ifndef STARBOARD_EXPORT_H_
#define STARBOARD_EXPORT_H_

#include "starboard/configuration.h"

// COMPONENT_BUILD is defined when generating shared libraries for each project,
// rather than static libraries. This means we need to be careful about
// EXPORT/IMPORT.
//
// SB_IS_EVERGREEN is defined when the binaries generated will be composed
// entirely of the Starboard implementation and will provide the Starboard API,
// with all client applications being built separately.

#if defined(COMPONENT_BUILD) || SB_IS(EVERGREEN)

// STARBOARD_IMPLEMENTATION is defined when building the Starboard library
// sources, and shouldn't be defined when building sources that are clients of
// Starboard.

#if defined(STARBOARD_IMPLEMENTATION)

// Specification for a symbol that should be exported when building the DLL and
// imported when building code that uses the DLL.
#define SB_EXPORT SB_EXPORT_PLATFORM

// Specification for a symbol that should be exported or imported for testing
// purposes only.
#define SB_EXPORT_PRIVATE SB_EXPORT_PLATFORM

// Specification for a symbol that is expected to be defined externally to this
// module.
#define SB_IMPORT SB_IMPORT_PLATFORM

#else  // defined(STARBOARD_IMPLEMENTATION)
#define SB_EXPORT SB_IMPORT_PLATFORM
#define SB_EXPORT_PRIVATE SB_IMPORT_PLATFORM
#define SB_IMPORT SB_EXPORT_PLATFORM
#endif
#else  // defined(COMPONENT_BUILD) || SB_IS(EVERGREEN)
#define SB_EXPORT
#define SB_EXPORT_PRIVATE
#define SB_IMPORT
#endif  // defined(COMPONENT_BUILD)

#endif  // STARBOARD_EXPORT_H_
