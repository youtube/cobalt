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

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_WIN_WIN32_LIB_CONFIGURATION_PUBLIC_H_
#define STARBOARD_WIN_WIN32_LIB_CONFIGURATION_PUBLIC_H_

#include "starboard/win/shared/configuration_public.h"

// Ensure the scene is re-rasterized even if the render tree is unchanged so
// that headset look changes are properly rendered.
#undef SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER
#define SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER 1

#endif  // STARBOARD_WIN_WIN32_LIB_CONFIGURATION_PUBLIC_H_
