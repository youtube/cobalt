/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/base/wrap_main.h"
#include "cobalt/browser/application.h"
#if defined(OS_STARBOARD)
#include "cobalt/browser/starboard/event_handler.h"
#include "cobalt/system_window/starboard/system_window.h"
#endif

namespace {

cobalt::browser::Application* g_application = NULL;

void StartApplication(int /*argc*/, char** /*argv*/, const char* /*link*/,
                      const base::Closure& quit_closure) {
  DCHECK(!g_application);
  g_application = cobalt::browser::CreateApplication(quit_closure).release();
  DCHECK(g_application);
}

void StopApplication() {
  DCHECK(g_application);
  delete g_application;
  g_application = NULL;
}

}  // namespace

#if defined(OS_STARBOARD)
COBALT_WRAP_EVENT_MAIN(StartApplication,
                       cobalt::browser::EventHandler::HandleEvent,
                       StopApplication);
#else
COBALT_WRAP_BASE_MAIN(StartApplication, StopApplication);
#endif
