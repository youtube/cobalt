// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

// Defines all the "content_shell" command-line switches.

#ifndef COBALT_SHELL_COMMON_SHELL_SWITCHES_H_
#define COBALT_SHELL_COMMON_SHELL_SWITCHES_H_

#include "build/build_config.h"

namespace switches {

<<<<<<< HEAD
extern const char kContentShellUserDataDir[];
=======
constexpr char kDefaultURL[] = "https://www.youtube.com/tv";

extern const char kContentShellDataPath[];
>>>>>>> df244d7077b (evergreen: Implement lightweight device authentication (#7154))
extern const char kCrashDumpsDir[];
extern const char kDisableSystemFontCheck[];
extern const char kContentShellHostWindowSize[];
extern const char kContentShellHideToolbar[];
extern const char kIsolatedContextOrigins[];
extern const char kOmitDeviceAuthenticationQueryParameters[];
extern const char kRemoteDebuggingAddress[];
extern const char kTestRegisterStandardScheme[];

}  // namespace switches

#endif  // COBALT_SHELL_COMMON_SHELL_SWITCHES_H_
