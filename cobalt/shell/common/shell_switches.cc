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

#include "cobalt/shell/common/shell_switches.h"

#include "base/command_line.h"

namespace switches {

// Makes Content Shell use the given path for its data directory.
// NOTE: If changing this value, change the corresponding Java-side value in
// ContentShellBrowserTestActivity.java#getUserDataDirectoryCommandLineSwitch()
// to match.
const char kContentShellDataPath[] = "data-path";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Disables showing splash screen.
const char kDisableSplashScreen[] = "disable-splash-screen";

// Disables the check for the system font when specified.
const char kDisableSystemFontCheck[] = "disable-system-font-check";

// Size for the content_shell's host window (i.e. "800x600").
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";

// Hides toolbar from content_shell's host window.
const char kContentShellHideToolbar[] = "content-shell-hide-toolbar";

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Let DevTools front-end talk to the target of type "tab" rather than
// "frame" when inspecting a WebContents.
const char kContentShellDevToolsTabTarget[] =
    "content-shell-devtools-tab-target";
#endif

// Force to display a static image as splash screen.
const char kForceImageSplashScreen[] = "force-image-splash-screen";

// Enables APIs guarded with the [IsolatedContext] IDL attribute for the given
// comma-separated list of origins.
const char kIsolatedContextOrigins[] = "isolated-context-origins";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Note that the remote debugging protocol does not
// perform any authentication, so exposing it too widely can be a security
// risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// The delay in milliseconds before shutting down the splash screen.
const char kSplashScreenShutdownDelayMs[] = "splash-screen-shutdown-delay-ms";

bool ShouldCreateSplashScreen() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  return !command_line->HasSwitch(kDisableSplashScreen);
}

}  // namespace switches
