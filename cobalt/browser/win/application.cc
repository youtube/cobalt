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

#include "cobalt/browser/application.h"

#include "cobalt/system_window/application_event.h"

namespace cobalt {
namespace browser {

class ApplicationWin : public Application {
 public:
  ApplicationWin();
  ~ApplicationWin() OVERRIDE;

  void OnApplicationEvent(const base::Event* event);

 private:
  base::EventCallback event_callback_;
};

scoped_ptr<Application> CreateApplication() {
  return scoped_ptr<Application>(new ApplicationWin());
}

ApplicationWin::ApplicationWin() {
  event_callback_ =
      base::Bind(&ApplicationWin::OnApplicationEvent, base::Unretained(this));
  event_dispatcher_.AddEventCallback(system_window::ApplicationEvent::TypeId(),
                                     event_callback_);
}

ApplicationWin::~ApplicationWin() {
  event_dispatcher_.RemoveEventCallback(
      system_window::ApplicationEvent::TypeId(), event_callback_);
}

void ApplicationWin::OnApplicationEvent(const base::Event* event) {
  const system_window::ApplicationEvent* application_event =
      base::polymorphic_downcast<const system_window::ApplicationEvent*>(event);
  if (application_event->type() == system_window::ApplicationEvent::kQuit) {
    Quit();
  }
}


}  // namespace browser
}  // namespace cobalt
