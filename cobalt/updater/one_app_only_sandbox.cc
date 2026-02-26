// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

// This is a test app for Evergreen that only works when it loads the
// evergreen-cert-test.html page.

#include <array>
#include <sstream>
#include <string>
#include <vector>

#include "base/allocator/partition_allocator/memory_reclaimer.h"
#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cobalt/app/app_lifecycle_delegate.h"
#include "cobalt/browser/switches.h"
#include "starboard/event.h"
#include "starboard/loader_app/app_key.h"
#include "starboard/system.h"

#include <init_musl.h>

namespace {

const char kMainAppKey[] = "aHR0cHM6Ly93d3cueW91dHViZS5jb20vdHY=";
// Use the html instead of url or app key to target the Evergreen cert test
// page, because there are various urls to launch the test page.
const char kEvergreenCertTestHtml[] = "evergreen-cert-test.html";

bool is_target_app = false;

}  // namespace

void SbEventHandle(const SbEvent* event) {
  if (!is_target_app && (event->type == kSbEventTypePreload ||
                         event->type == kSbEventTypeStart)) {
    const SbEventStartData* data = static_cast<SbEventStartData*>(event->data);
    const base::CommandLine command_line(
        data->argument_count, const_cast<const char**>(data->argument_values));

    if (command_line.HasSwitch(cobalt::switches::kInitialURL)) {
      std::string url =
          command_line.GetSwitchValueASCII(cobalt::switches::kInitialURL);
      if (loader_app::GetAppKey(url) != kMainAppKey &&
          url.find(kEvergreenCertTestHtml) == std::string::npos) {
        // If the app is not the main app nor Evergreen cert test page, stop the
        // app.
        SbSystemRequestStop(0);
        return;
      }
    }

    is_target_app = true;
  }

  // This object's lifetime extends beyond the function's lifetime, until the
  // function is called with kSbEventTypeStop at some time in the future.
  static cobalt::AppLifecycleDelegate* s_lifecycle_delegate = nullptr;
  if (!s_lifecycle_delegate) {
    s_lifecycle_delegate = new cobalt::AppLifecycleDelegate();
  }
  s_lifecycle_delegate->HandleEvent(event);
  if (event->type == kSbEventTypeStop) {
    delete s_lifecycle_delegate;
    s_lifecycle_delegate = nullptr;
  }
}
