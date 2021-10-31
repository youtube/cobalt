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

// The Starboard configuration for Desktop x64 Linux. Other devices will have
// specific Starboard implementations, even if they ultimately are running some
// version of Linux.

// Other source files should never include this header directly, but should
// include the generic "starboard/configuration.h" instead.

// This file is meant to live under starboard/linux/stadia/ (see ../README.md),
// so we disable the header guard check.
// NOLINT(build/header_guard)

#ifndef STARBOARD_LINUX_STADIA_CONFIGURATION_PUBLIC_H_
#define STARBOARD_LINUX_STADIA_CONFIGURATION_PUBLIC_H_

// Include the x64x11 Linux configuration on which this config is based.
#include "starboard/linux/x64x11/configuration_public.h"

#endif  // STARBOARD_LINUX_STADIA_CONFIGURATION_PUBLIC_H_
