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

#include <cstddef>

namespace switches {

inline constexpr char kDefaultURL[] = "https://www.youtube.com/tv";
inline constexpr char kSplashScreenURL[] = "h5vcc-embedded://splash.html";
inline constexpr char16_t kDefaultSplashCacheName[] = u"default";
// 10MB limit for splash video to prevent memory exhaustion.
// Current cobalt/shell/embedded_resources/loader_embedded_resources/splash.webm
// is around 20KB, so 10MB should be sufficient.
inline constexpr size_t kMaxSplashContentSize = 10 * 1024 * 1024;

extern const char kContentShellUserDataDir[];
extern const char kCrashDumpsDir[];
extern const char kDisableSplashScreen[];
extern const char kDisableSystemFontCheck[];
extern const char kContentShellHostWindowSize[];
extern const char kContentShellHideToolbar[];
extern const char kIsolatedContextOrigins[];
extern const char kOmitDeviceAuthenticationQueryParameters[];
extern const char kRemoteDebuggingAddress[];
extern const char kSplashScreenShutdownDelayMs[];
extern const char kTestRegisterStandardScheme[];

// Checks if the splash screen should be created.
// Returns false if kDisableSplashScreen is present.
bool ShouldCreateSplashScreen();

}  // namespace switches

#endif  // COBALT_SHELL_COMMON_SHELL_SWITCHES_H_
