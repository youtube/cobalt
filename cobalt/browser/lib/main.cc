// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "cobalt/browser/lib/imported/main.h"
#include "cobalt/browser/starboard/event_handler.h"
#include "starboard/event.h"
#include "starboard/input.h"

namespace {

cobalt::browser::Application* g_application = NULL;

void StartApplication(int /*argc*/, char** /*argv*/, const char* /*link*/,
                      const base::Closure& quit_closure) {
  DCHECK(!g_application);
  LOG(INFO) << "Starting application!";
  g_application = cobalt::browser::CreateApplication(quit_closure).release();
  DCHECK(g_application);
  CbLibOnCobaltInitialized();
}

void StopApplication() {
  DCHECK(g_application);
  LOG(INFO) << "Stopping application!";
  delete g_application;
  g_application = NULL;
}

void HandleEvent(const SbEvent* starboard_event) {
  if (!CbLibHandleEvent(starboard_event)) {
    cobalt::browser::EventHandler::HandleEvent(starboard_event);
  }
}

}  // namespace

COBALT_WRAP_EVENT_MAIN(StartApplication,
                       HandleEvent,
                       StopApplication);
