/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/h5vcc/h5vcc_runtime.h"
#include "cobalt/system_window/application_event.h"

namespace cobalt {
namespace h5vcc {
H5vccRuntime::H5vccRuntime(base::EventDispatcher* event_dispatcher)
    : event_dispatcher_(event_dispatcher) {
  on_pause_ = new H5vccRuntimeEventTarget;
  on_resume_ = new H5vccRuntimeEventTarget;

  DCHECK(event_dispatcher_);
  application_event_callback_ =
      base::Bind(&H5vccRuntime::OnApplicationEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(system_window::ApplicationEvent::TypeId(),
                                      application_event_callback_);
}

H5vccRuntime::~H5vccRuntime() {
  event_dispatcher_->RemoveEventCallback(
      system_window::ApplicationEvent::TypeId(), application_event_callback_);
}

const scoped_refptr<H5vccRuntimeEventTarget>& H5vccRuntime::on_pause() const {
  return on_pause_;
}

const scoped_refptr<H5vccRuntimeEventTarget>& H5vccRuntime::on_resume() const {
  return on_resume_;
}

void H5vccRuntime::OnApplicationEvent(const base::Event* event) {
  const system_window::ApplicationEvent* app_event =
      base::polymorphic_downcast<const system_window::ApplicationEvent*>(event);
  if (app_event->type() == system_window::ApplicationEvent::kSuspend) {
    DLOG(INFO) << "Got pause event.";
    on_pause()->DispatchEvent();
  } else if (app_event->type() == system_window::ApplicationEvent::kResume) {
    DLOG(INFO) << "Got resume event.";
    on_resume()->DispatchEvent();
  }
}
}  // namespace h5vcc
}  // namespace cobalt
