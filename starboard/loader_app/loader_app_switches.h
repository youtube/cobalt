// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_LOADER_APP_LOADER_APP_SWITCHES_H_
#define STARBOARD_LOADER_APP_LOADER_APP_SWITCHES_H_

#include "starboard/configuration.h"

namespace starboard {
namespace loader_app {

// Absolute path to the alternative content directory to be used.
// The flags allows for system apps with content defined on the
// device which is not part of the Evergreen updates.
extern const char kContent[];

// Initial URL for the web app.
// If not specified the default is to load the YouTube main app.
extern const char kURL[];

// Run Evergreen Lite by loading the system image and disabling the updater.
extern const char kEvergreenLite[];

// Print the loader_app version on the command line.
extern const char kLoaderAppVersion[];

// Print the loader_app Starboard ABI string on the command line.
extern const char kShowSABI[];

}  // namespace loader_app
}  // namespace starboard

#endif  // STARBOARD_LOADER_APP_LOADER_APP_SWITCHES_H_
