// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/h5vcc/h5vcc_runtime.h"

#include "cobalt/base/application_event.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace h5vcc {
H5vccRuntime::H5vccRuntime(base::EventDispatcher* event_dispatcher,
                           const std::string& initial_deep_link)
    : event_dispatcher_(event_dispatcher) {
  initial_deep_link_ = initial_deep_link;
  on_deep_link_ = new H5vccDeepLinkEventTarget;
  on_pause_ = new H5vccRuntimeEventTarget;
  on_resume_ = new H5vccRuntimeEventTarget;

  DCHECK(event_dispatcher_);
  application_event_callback_ =
      base::Bind(&H5vccRuntime::OnApplicationEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(base::ApplicationEvent::TypeId(),
                                      application_event_callback_);
  deep_link_event_callback_ =
      base::Bind(&H5vccRuntime::OnDeepLinkEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(base::DeepLinkEvent::TypeId(),
                                      deep_link_event_callback_);
}

H5vccRuntime::~H5vccRuntime() {
  event_dispatcher_->RemoveEventCallback(base::ApplicationEvent::TypeId(),
                                         application_event_callback_);
  event_dispatcher_->RemoveEventCallback(base::DeepLinkEvent::TypeId(),
                                         deep_link_event_callback_);
}

const std::string& H5vccRuntime::initial_deep_link() const {
  return initial_deep_link_;
}

const scoped_refptr<H5vccDeepLinkEventTarget>& H5vccRuntime::on_deep_link()
    const {
  return on_deep_link_;
}

const scoped_refptr<H5vccRuntimeEventTarget>& H5vccRuntime::on_pause() const {
  return on_pause_;
}

const scoped_refptr<H5vccRuntimeEventTarget>& H5vccRuntime::on_resume() const {
  return on_resume_;
}

void H5vccRuntime::TraceMembers(script::Tracer* tracer) {
  tracer->Trace(on_deep_link_);
  tracer->Trace(on_pause_);
  tracer->Trace(on_resume_);
}

void H5vccRuntime::OnApplicationEvent(const base::Event* event) {
  const base::ApplicationEvent* app_event =
      base::polymorphic_downcast<const base::ApplicationEvent*>(event);
  if (app_event->type() == kSbEventTypePause) {
    on_pause()->DispatchEvent();
  } else if (app_event->type() == kSbEventTypeUnpause) {
    on_resume()->DispatchEvent();
  }
}

void H5vccRuntime::OnDeepLinkEvent(const base::Event* event) {
  const base::DeepLinkEvent* deep_link_event =
      base::polymorphic_downcast<const base::DeepLinkEvent*>(event);
  if (!deep_link_event->IsH5vccLink()) {
    DLOG(INFO) << "Got deep link event: " << deep_link_event->link();
    on_deep_link()->DispatchEvent(deep_link_event->link());
  }
}
}  // namespace h5vcc
}  // namespace cobalt
