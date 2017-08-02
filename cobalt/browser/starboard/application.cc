// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/application.h"

#include "base/memory/scoped_ptr.h"
#include "cobalt/browser/starboard/event_handler.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {

class ApplicationStarboard : public Application {
 public:
  ApplicationStarboard(const base::Closure& quit_closure, bool should_preload)
      : Application(quit_closure, should_preload),
        event_handler_(&event_dispatcher_) {}

 private:
  // Event handler to receive Starboard events, convert to Cobalt events
  // and dispatch to the rest of the system.
  EventHandler event_handler_;
};

scoped_ptr<Application> CreateApplication(const base::Closure& quit_closure) {
  return scoped_ptr<Application>(
      new ApplicationStarboard(quit_closure, false /*should_preload*/));
}

scoped_ptr<Application> PreloadApplication(const base::Closure& quit_closure) {
  return scoped_ptr<Application>(
      new ApplicationStarboard(quit_closure, true /*should_preload*/));
}

}  // namespace browser
}  // namespace cobalt
