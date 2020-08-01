// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "base/synchronization/lock.h"
#include "cobalt/base/deep_link_event.h"
#include "cobalt/base/polymorphic_downcast.h"

namespace cobalt {
namespace h5vcc {
H5vccRuntime::H5vccRuntime(base::EventDispatcher* event_dispatcher)
    : event_dispatcher_(event_dispatcher) {
  on_deep_link_ = new H5vccDeepLinkEventTarget(
      base::Bind(&H5vccRuntime::GetUnconsumedDeepLink, base::Unretained(this)));
  on_pause_ = new H5vccRuntimeEventTarget;
  on_resume_ = new H5vccRuntimeEventTarget;

  DCHECK(event_dispatcher_);
  deep_link_event_callback_ =
      base::Bind(&H5vccRuntime::OnDeepLinkEvent, base::Unretained(this));
  event_dispatcher_->AddEventCallback(base::DeepLinkEvent::TypeId(),
                                      deep_link_event_callback_);
}

H5vccRuntime::~H5vccRuntime() {
  event_dispatcher_->RemoveEventCallback(base::DeepLinkEvent::TypeId(),
                                         deep_link_event_callback_);
}

std::string H5vccRuntime::initial_deep_link() {
  base::AutoLock auto_lock(lock_);
  if (consumed_callback_) {
    std::move(consumed_callback_).Run();
  }
  if (!initial_deep_link_.empty()) {
    LOG(INFO) << "Returning stashed deep link: " << initial_deep_link_;
  }
  return initial_deep_link_;
}

const std::string& H5vccRuntime::GetUnconsumedDeepLink() {
  base::AutoLock auto_lock(lock_);
  if (!initial_deep_link_.empty() && consumed_callback_) {
    LOG(INFO) << "Returning stashed unconsumed deep link: "
              << initial_deep_link_;
    std::move(consumed_callback_).Run();
    return initial_deep_link_;
  }
  return empty_string_;
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

void H5vccRuntime::OnDeepLinkEvent(const base::Event* event) {
  base::AutoLock auto_lock(lock_);
  const base::DeepLinkEvent* deep_link_event =
      base::polymorphic_downcast<const base::DeepLinkEvent*>(event);
  const std::string& link = deep_link_event->link();
  if (link.empty()) {
    return;
  }

  if (on_deep_link()->empty()) {
    // Store the link for later consumption.
    LOG(INFO) << "Stashing deep link: " << link;
    initial_deep_link_ = link;
    consumed_callback_ = deep_link_event->callback();
  } else {
    // Send the link.
    DCHECK(consumed_callback_.is_null());
    LOG(INFO) << "Dispatching deep link event: " << link;
    on_deep_link()->DispatchEvent(link);
    base::OnceClosure callback(deep_link_event->callback());
    if (callback) {
      std::move(callback).Run();
    }
  }
}
}  // namespace h5vcc
}  // namespace cobalt
