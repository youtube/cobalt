// Copyright 2016 Google Inc. All Rights Reserved.
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

// The Starboard configuration for Desktop X64 Linux configured for the future
// starboard API.

#ifndef STARBOARD_LINUX_X64DIRECTFB_FUTURE_CONFIGURATION_PUBLIC_H_
#define STARBOARD_LINUX_X64DIRECTFB_FUTURE_CONFIGURATION_PUBLIC_H_

// Include the X64DIRECTFB Linux configuration.
#include "starboard/linux/x64directfb/configuration_public.h"

// The API version implemented by this platform.
#undef SB_API_VERSION
#define SB_API_VERSION SB_EXPERIMENTAL_API_VERSION

#endif  // STARBOARD_LINUX_X64DIRECTFB_FUTURE_CONFIGURATION_PUBLIC_H_
