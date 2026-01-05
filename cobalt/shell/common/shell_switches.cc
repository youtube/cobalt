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

namespace switches {

// Makes Content Shell use the given path for its data directory.
// NOTE: "user-data-dir" is used to align with Chromedriver's behavior. Please
// do NOT change this to another value.
// NOTE: The same value is also used at Java-side in
// ContentShellBrowserTestActivity.java#getUserDataDirectoryCommandLineSwitch().
const char kContentShellUserDataDir[] = "user-data-dir";

// The directory breakpad should store minidumps in.
const char kCrashDumpsDir[] = "crash-dumps-dir";

// Disables the check for the system font when specified.
const char kDisableSystemFontCheck[] = "disable-system-font-check";

// Size for the content_shell's host window (i.e. "800x600").
const char kContentShellHostWindowSize[] = "content-shell-host-window-size";

// Hides toolbar from content_shell's host window.
const char kContentShellHideToolbar[] = "content-shell-hide-toolbar";

// Enables APIs guarded with the [IsolatedContext] IDL attribute for the given
// comma-separated list of origins.
const char kIsolatedContextOrigins[] = "isolated-context-origins";

// When set, no device authentication parameters will be appended to the initial
// URL."
const char kOmitDeviceAuthenticationQueryParameters[] =
    "omit-device-authentication-query-parameters";

// Use the given address instead of the default loopback for accepting remote
// debugging connections. Note that the remote debugging protocol does not
// perform any authentication, so exposing it too widely can be a security
// risk.
const char kRemoteDebuggingAddress[] = "remote-debugging-address";

// The delay in milliseconds before shutting down the splash screen.
const char kSplashScreenShutdownDelayMs[] = "splash-screen-shutdown-delay-ms";

// Register the provided scheme as a standard scheme.
const char kTestRegisterStandardScheme[] = "test-register-standard-scheme";

}  // namespace switches
