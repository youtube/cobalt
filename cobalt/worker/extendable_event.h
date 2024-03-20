// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WORKER_EXTENDABLE_EVENT_H_
#define COBALT_WORKER_EXTENDABLE_EVENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "cobalt/base/token.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/v8c/native_promise.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/script/wrappable.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt/web/event.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/extendable_event_init.h"
#include "cobalt/worker/service_worker_context.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "v8/include/v8.h"

namespace cobalt {
namespace worker {

class ExtendableEvent : public web::Event,
                        public web::Context::EnvironmentSettingsChangeObserver {
 public:
  explicit ExtendableEvent(script::EnvironmentSettings* settings,
                           const std::string& type)
      : Event(type), isolate_(web::get_isolate(settings)) {
    InitializeEnvironmentSettingsChangeObserver(settings);
  }
  ExtendableEvent(script::EnvironmentSettings* settings,
                  const std::string& type, const ExtendableEventInit& init_dict)
      : ExtendableEvent(settings, base_token::Token(type), init_dict) {}
  ExtendableEvent(script::EnvironmentSettings* settings, base_token::Token type,
                  base::OnceCallback<void(bool)> done_callback =
                      base::OnceCallback<void(bool)>())
      : Event(type),
        done_callback_(std::move(done_callback)),
        isolate_(web::get_isolate(settings)) {
    InitializeEnvironmentSettingsChangeObserver(settings);
  }
  ExtendableEvent(script::EnvironmentSettings* settings, base_token::Token type,
                  const ExtendableEventInit& init_dict)
      : Event(type, init_dict), isolate_(web::get_isolate(settings)) {
    InitializeEnvironmentSettingsChangeObserver(settings);
  }

  // web::Context::EnvironmentSettingsChangeObserver
  void OnEnvironmentSettingsChanged(bool context_valid) override;

  void WaitUntil(
      script::EnvironmentSettings* settings,
      std::unique_ptr<script::Promise<script::ValueHandle*>>& promise,
      script::ExceptionState* exception_state);

  void StateChange(script::EnvironmentSettings* settings,
                   const script::Promise<script::ValueHandle*>* promise);

  bool IsActive() {
    // An ExtendableEvent object is said to be active when its timed out flag
    // is unset and either its pending promises count is greater than zero or
    // its dispatch flag is set.
    //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#extendableevent-active
    return !timed_out_flag_ &&
           ((pending_promise_count_ > 0) || IsBeingDispatched());
  }

  bool has_rejected_promise() const { return has_rejected_promise_; }

  DEFINE_WRAPPABLE_TYPE(ExtendableEvent);

 protected:
  ~ExtendableEvent() override {
    std::move(remove_environment_settings_change_observer_).Run();
  }

 private:
  void ExtendLifetime(const script::Promise<script::ValueHandle*>* promise);
  void LessenLifetime(const script::Promise<script::ValueHandle*>* promise);
  void InitializeEnvironmentSettingsChangeObserver(
      script::EnvironmentSettings* settings);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#extendableevent-extend-lifetime-promises
  // std::list<script::Promise<script::ValueHandle*>> extend_lifetime_promises_;
  int pending_promise_count_ = 0;
  bool has_rejected_promise_ = false;
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#extendableevent-timed-out-flag
  bool timed_out_flag_ = false;

  base::OnceCallback<void(bool)> done_callback_;
  base::OnceClosure remove_environment_settings_change_observer_;
  v8::Isolate* isolate_;

  std::map<const script::Promise<script::ValueHandle*>*,
           v8::TracedGlobal<v8::Value>*>
      traced_global_promises_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_EXTENDABLE_EVENT_H_
