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

#ifndef COBALT_BINDINGS_SHARED_IDL_CONDITIONAL_MACROS_H_
#define COBALT_BINDINGS_SHARED_IDL_CONDITIONAL_MACROS_H_

#include "starboard/configuration.h"

// Define preprocessor macros that cannot be determined at GYP time for use in
// IDL files with Conditionals (e.g. conditional interface definitions, or
// conditional attributes). This is necessary to make macros for IDL
// Conditionals that are dependent on Starboard feature macros that get defined
// in header files.

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
// This is used to conditionally define the On Screen Keyboard interface and
// attribute.
#define COBALT_ENABLE_ON_SCREEN_KEYBOARD
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

// This is used to conditionally define setMaxVideoCapabilities() in
// HTMLVideoElement.
#define COBALT_ENABLE_SET_MAX_VIDEO_CAPABILITIES

#endif  // COBALT_BINDINGS_SHARED_IDL_CONDITIONAL_MACROS_H_
