// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

// The Starboard configuration for Desktop x64 Linux. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Linux.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

#ifndef STARBOARD_LINUX_X64X11_CONFIGURATION_PUBLIC_H_
#define STARBOARD_LINUX_X64X11_CONFIGURATION_PUBLIC_H_

// --- Architecture Configuration --------------------------------------------

// Configuration parameters that allow the application to make some general
// compile-time decisions with respect to the the number of cores likely to be
// available on this platform. For a definitive measure, the application should
// still call SbSystemGetNumberOfProcessors at runtime.

// --- Shared Configuration and Overrides ------------------------------------

// Include the Linux configuration that's common between all Desktop Linuxes.
#include "starboard/linux/shared/configuration_public.h"

#endif  // STARBOARD_LINUX_X64X11_CONFIGURATION_PUBLIC_H_
