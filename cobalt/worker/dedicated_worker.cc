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

#include "cobalt/browser/stack_size_constants.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"
#include "cobalt/worker/worker.h"
#include "cobalt/worker/worker_options.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL,
                                 script::ExceptionState* exception_state)
    : web::EventTarget(settings), script_url_(scriptURL) {
  Initialize(exception_state);
}

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL,
                                 const WorkerOptions& worker_options,
                                 script::ExceptionState* exception_state)
    : web::EventTarget(settings),
      script_url_(scriptURL),
      worker_options_(worker_options) {
  Initialize(exception_state);
}

void DedicatedWorker::Initialize(script::ExceptionState* exception_state) {
  // Algorithm for the Worker constructor.
  //   https://html.spec.whatwg.org/commit-snapshots/465a6b672c703054de278b0f8133eb3ad33d93f4/#dom-worker

  // 1. The user agent may throw a "SecurityError" web::DOMException if the
  //    request violates a policy decision (e.g. if the user agent is configured
  //    to not allow the page to start dedicated workers).
  // 2. Let outside settings be the current settings object.
  // 3. Parse the scriptURL argument relative to outside settings.
  Worker::Options options;
  const GURL& base_url = environment_settings()->base_url();
  options.url = base_url.Resolve(script_url_);

  // 4. If this fails, throw a "SyntaxError" web::DOMException.
  if (!options.url.is_valid()) {
    web::DOMException::Raise(
        web::DOMException::kSyntaxErr,
        script_url_ + " cannot be resolved based on " + base_url.spec() + ".",
        exception_state);
    return;
  }

  // 5. Let worker URL be the resulting URL record.
  options.web_options.stack_size = cobalt::browser::kWorkerStackSize;
  options.web_options.web_settings =
      environment_settings()->context()->web_settings();
  options.web_options.network_module =
      environment_settings()->context()->network_module();

  // Propagate the CSP Options from the current environment settings.
  options.global_scope_options.csp_options =
      environment_settings()
          ->context()
          ->GetWindowOrWorkerGlobalScope()
          ->options()
          .csp_options;

  // 6. Let worker be a new Worker object.
  // 7. Let outside port be a new MessagePort in outside settings's Realm.
  // 8. Associate the outside port with worker.
  outside_port_ = new web::MessagePort();
  outside_port_->EntangleWithEventTarget(this);
  // 9. Run this step in parallel:
  //    1. Run a worker given worker, worker URL, outside settings, outside
  //    port, and options.
  options.outside_context = environment_settings()->context();
  options.outside_event_target = this;
  options.outside_port = outside_port_.get();
  options.options = worker_options_;
  options.web_options.service_worker_context =
      options.outside_context->service_worker_context();
  // Store the current source location as the construction location, to be used
  // in the error event if worker loading of initialization fails.
  auto stack_trace =
      options.outside_context->global_environment()->GetStackTrace(0);
  if (stack_trace.size() > 0) {
    options.construction_location = base::SourceLocation(
        stack_trace[0].source_url, stack_trace[0].line_number,
        stack_trace[0].column_number);
  } else {
    options.construction_location.file_path =
        environment_settings()->creation_url().spec();
  }
  worker_.reset(new Worker(WorkerConsts::kDedicatedWorkerName, options));
  // 10. Return worker.
}

DedicatedWorker::~DedicatedWorker() { Terminate(); }

void DedicatedWorker::Terminate() {
  if (worker_) {
    worker_->Terminate();
  }
}

// void postMessage(any message, object transfer);
// -> void PostMessage(const script::ValueHandleHolder& message,
//                     script::Sequence<script::ValueHandle*> transfer) {}
void DedicatedWorker::PostMessage(const script::ValueHandleHolder& message) {
  DCHECK(worker_);
  worker_->PostMessage(message);
}


}  // namespace worker
}  // namespace cobalt
