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

#include "cobalt/worker/dedicated_worker.h"

#include <memory>
#include <string>

#include "cobalt/dom/event_target.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/worker/message_port.h"
#include "cobalt/worker/worker.h"
#include "cobalt/worker/worker_options.h"

namespace cobalt {
namespace worker {

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL)
    : EventTarget(settings), settings_(settings), script_url_(scriptURL) {
  Initialize();
}

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL,
                                 const WorkerOptions& options)
    : EventTarget(settings),
      settings_(settings),
      script_url_(scriptURL),
      options_(options) {
  Initialize();
}

void DedicatedWorker::Initialize() {
  // https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dom-worker

  // 1. The user agent may throw a "SecurityError" DOMException if the request
  //    violates a policy decision (e.g. if the user agent is configured to
  //    not
  //    allow the page to start dedicated workers).
  // 2. Let outside settings be the current settings object.
  // 3. Parse the scriptURL argument relative to outside settings.
  // 4. If this fails, throw a "SyntaxError" DOMException.
  // 5. Let worker URL be the resulting URL record.
  std::string worker_url = script_url_;
  // 6. Let worker be a new Worker object.
  worker_.reset(new Worker);
  // 7. Let outside port be a new MessagePort in outside settings's Realm.
  // 8. Associate the outside port with worker.
  outside_port_.reset(new MessagePort(this, settings_));
  //    1. Run a worker given worker, worker URL, outside settings, outside
  //    port, and options.
  worker_->RunDedicated(worker_url, settings_, outside_port_.get(), options_);
  // 10. Return worker.
}

DedicatedWorker::~DedicatedWorker() { Terminate(); }

void DedicatedWorker::Terminate() {}

// void postMessage(any message, object transfer);
// -> void PostMessage(const script::ValueHandleHolder& message,
//                     script::Sequence<script::ValueHandle*> transfer) {}
void DedicatedWorker::PostMessage(
    const script::ValueHandleHolder& message,
    const script::Sequence<std::string>& options) {
  if (worker_ && worker_->inside_port())
    worker_->inside_port()->PostMessage(message, options);
}


}  // namespace worker
}  // namespace cobalt
