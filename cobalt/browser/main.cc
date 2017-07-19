// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/browser/application.h"

namespace {

cobalt::browser::Application* g_application = NULL;

void PreloadApplication(int /*argc*/, char** /*argv*/, const char* /*link*/,
                        const base::Closure& quit_closure) {
  DCHECK(!g_application);
  g_application =
      new cobalt::browser::Application(quit_closure, true /*should_preload*/);
  DCHECK(g_application);
}

void StartApplication(int /*argc*/, char** /*argv*/, const char* /*link*/,
                      const base::Closure& quit_closure) {
  if (!g_application) {
    g_application = new cobalt::browser::Application(quit_closure,
                                                     false /*should_preload*/);
    DCHECK(g_application);
  } else {
    g_application->Start();
  }
}

void StopApplication() {
  DCHECK(g_application);
  delete g_application;
  g_application = NULL;
}

void HandleStarboardEvent(const SbEvent* starboard_event) {
  DCHECK(starboard_event);
  DCHECK(g_application);
  g_application->HandleStarboardEvent(starboard_event);
}

}  // namespace

#if defined(OS_STARBOARD)
COBALT_WRAP_MAIN(PreloadApplication, StartApplication, HandleStarboardEvent,
                 StopApplication);
#else
COBALT_WRAP_BASE_MAIN(StartApplication, StopApplication);
#endif
