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

#include "cobalt/worker/service_worker.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/value_handle.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/extendable_message_event.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_state.h"

namespace cobalt {
namespace worker {

ServiceWorker::ServiceWorker(script::EnvironmentSettings* settings,
                             worker::ServiceWorkerObject* worker)
    : web::EventTarget(settings),
      worker_(worker),
      state_(kServiceWorkerStateParsed) {}


void ServiceWorker::PostMessage(const script::ValueHandleHolder& message) {
  // https://w3c.github.io/ServiceWorker/#service-worker-postmessage-options
  web::EventTarget* event_target = worker_->worker_global_scope();
  if (!event_target) return;

  base::MessageLoop* message_loop =
      event_target->environment_settings()->context()->message_loop();
  if (!message_loop) {
    return;
  }
  std::unique_ptr<script::DataBuffer> data_buffer(
      script::SerializeScriptValue(message));
  if (!data_buffer) {
    return;
  }
  message_loop->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](web::EventTarget* event_target,
                        std::unique_ptr<script::DataBuffer> data_buffer) {
                       event_target->DispatchEvent(new ExtendableMessageEvent(
                           base::Tokens::message(), std::move(data_buffer)));
                     },
                     base::Unretained(event_target), std::move(data_buffer)));
}

}  // namespace worker
}  // namespace cobalt
