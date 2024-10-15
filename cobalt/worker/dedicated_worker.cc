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
#include "cobalt/dom/dom_settings.h"
#include "cobalt/dom/media_settings.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event_target.h"
#include "cobalt/web/message_port.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/worker.h"
#include "cobalt/worker/worker_options.h"
#include "cobalt/worker/worker_settings.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

namespace {
const dom::MediaSettings& GetMediaSettings(web::EnvironmentSettings* settings) {
  DCHECK(settings);
  DCHECK(settings->context());
  DCHECK(settings->context()->web_settings());
  const auto& web_settings = settings->context()->web_settings();
  return web_settings->media_settings();
}
// If this function returns true, MediaSource::GetSeekable() will short-circuit
// getting the buffered range from HTMLMediaElement by directly calling to
// MediaSource::GetBufferedRange(). This reduces potential cross-object,
// cross-thread calls between MediaSource and HTMLMediaElement.
// The default value is false.
bool IsMediaElementUsingMediaSourceBufferedRangeEnabled(
    web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings)
      .IsMediaElementUsingMediaSourceBufferedRangeEnabled()
      .value_or(false);
}

// If this function returns true, communication between HTMLMediaElement and
// MediaSource objects will be fully proxied between MediaSourceAttachment.
// The default value is false.
bool IsMediaElementUsingMediaSourceAttachmentMethodsEnabled(
    web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings)
      .IsMediaElementUsingMediaSourceAttachmentMethodsEnabled()
      .value_or(false);
}

// If this function returns true, experimental support for creating MediaSource
// objects in Dedicated Workers will be enabled. This also allows MSE handles
// to be transferred from Dedicated Workers back to the main thread.
// Requires MediaElement.EnableUsingMediaSourceBufferedRange and
// MediaElement.EnableUsingMediaSourceAttachmentMethods as prerequisites for
// this feature.
// The default value is false.
bool IsMseInWorkersEnabled(web::EnvironmentSettings* settings) {
  return GetMediaSettings(settings).IsMseInWorkersEnabled().value_or(false);
}
}  // namespace

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL,
                                 script::ExceptionState* exception_state)
    : web::EventTarget(settings), script_url_(scriptURL) {
  Initialize(settings, exception_state);
}

DedicatedWorker::DedicatedWorker(script::EnvironmentSettings* settings,
                                 const std::string& scriptURL,
                                 const WorkerOptions& worker_options,
                                 script::ExceptionState* exception_state)
    : web::EventTarget(settings),
      script_url_(scriptURL),
      worker_options_(worker_options) {
  Initialize(settings, exception_state);
}

void DedicatedWorker::Initialize(script::EnvironmentSettings* settings,
                                 script::ExceptionState* exception_state) {
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

  web::EnvironmentSettings* web_settings =
      base::polymorphic_downcast<web::EnvironmentSettings*>(settings);
  DCHECK(web_settings);
  if (IsMseInWorkersEnabled(web_settings) &&
      IsMediaElementUsingMediaSourceAttachmentMethodsEnabled(web_settings) &&
      IsMediaElementUsingMediaSourceBufferedRangeEnabled(web_settings)) {
    // DedicatedWorker constructor is only callable from the main Window.
    dom::DOMSettings* dom_settings =
        base::polymorphic_downcast<dom::DOMSettings*>(settings);
    DCHECK(dom_settings);
    DCHECK(dom_settings->can_play_type_handler());
    DCHECK(dom_settings->media_source_registry());
    options.can_play_type_handler = dom_settings->can_play_type_handler();
    options.media_source_registry = dom_settings->media_source_registry();
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
