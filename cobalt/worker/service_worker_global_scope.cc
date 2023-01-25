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

#include "cobalt/worker/service_worker_global_scope.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/script/environment_settings.h"
#include "cobalt/script/exception_state.h"
#include "cobalt/script/v8c/entry_scope.h"
#include "cobalt/web/environment_settings_helper.h"
#include "cobalt/worker/clients.h"
#include "cobalt/worker/fetch_event.h"
#include "cobalt/worker/fetch_event_init.h"
#include "cobalt/worker/service_worker_jobs.h"
#include "cobalt/worker/worker_settings.h"

namespace cobalt {
namespace worker {
ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    script::EnvironmentSettings* settings, ServiceWorkerObject* service_worker)
    : WorkerGlobalScope(settings),
      clients_(new Clients(settings)),
      service_worker_object_(base::AsWeakPtr(service_worker)) {
  loader::FetchInterceptorCoordinator::GetInstance()->Add(this);
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() {
  loader::FetchInterceptorCoordinator::GetInstance()->Clear();
}

void ServiceWorkerGlobalScope::Initialize() {}

void ServiceWorkerGlobalScope::ImportScripts(
    const std::vector<std::string>& urls,
    script::ExceptionState* exception_state) {
  // Algorithm for importScripts():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#importscripts

  // When the importScripts(urls) method is called, the user agent must import
  // scripts into worker global scope, with the following steps to perform the
  // fetch given the request request:
  // 1. Let serviceWorker be request’s client's global object's service
  //    worker.
  DCHECK(service_worker_object_);

  WorkerGlobalScope::ImportScriptsInternal(
      urls, exception_state,
      base::Bind(
          [](ServiceWorkerObject* service_worker, const GURL& url,
             script::ExceptionState* exception_state) -> std::string* {
            // 2. Let map be serviceWorker’s script resource map.
            DCHECK(service_worker);
            // 3. Let url be request’s url.
            DCHECK(url.is_valid());
            const ScriptResource* script_resource =
                service_worker->LookupScriptResource(url);
            std::string* resource_content =
                script_resource ? script_resource->content.get() : nullptr;
            // 4. If serviceWorker’s state is not "parsed" or "installing":
            if (service_worker->state() != kServiceWorkerStateParsed &&
                service_worker->state() != kServiceWorkerStateInstalling) {
              // 4.1. Return map[url] if it exists and a network error
              // otherwise.
              if (!resource_content) {
                web::DOMException::Raise(web::DOMException::kNetworkErr,
                                         exception_state);
              }
              return resource_content;
            }

            // 5. If map[url] exists:
            if (resource_content) {
              // 5.1. Append url to serviceWorker’s set of used scripts.
              service_worker->AppendToSetOfUsedScripts(url);

              // 5.2. Return map[url].
              return resource_content;
            }

            // 6. Let registration be serviceWorker’s containing service worker
            //    registration.
            // 7. Set request’s service-workers mode to "none".
            // 8. Set request’s cache mode to "no-cache" if any of the following
            //    are true:
            //    . registration’s update via cache mode is "none".
            //    . The current global object's force bypass cache for import
            //      scripts flag is set.
            //    . registration is stale.
            // 9. Let response be the result of fetching request.
            // TODO(b/228908203): Set request headers. This should probably be
            // a separate callback that gets passed to the ScriptLoader for the
            // FetcherFactory.

            return resource_content;
          },
          service_worker_object_),
      base::Bind(
          [](ServiceWorkerObject* service_worker, const GURL& url,
             std::string* content) {
            // 10. If response’s cache state is not "local", set registration’s
            //     last update check time to the current time.
            // 11. If response’s unsafe response is a bad import script
            //     response, then return a network error.

            // 12. Set map[url] to response.
            service_worker->SetScriptResource(url, content);
            // 13. Append url to serviceWorker’s set of used scripts.
            service_worker->AppendToSetOfUsedScripts(url);
            // 14. Set serviceWorker’s classic scripts imported flag.
            service_worker->set_classic_scripts_imported();
            // 15. Return response.
            return content;
          },
          service_worker_object_));
}

scoped_refptr<ServiceWorkerRegistration>
ServiceWorkerGlobalScope::registration() const {
  // Algorithm for registration():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-global-scope-registration

  // The registration getter steps are to return the result of getting the
  // service worker registration object representing this's service worker's
  // containing service worker registration in this's relevant settings object.
  DCHECK(service_worker_object_);
  return environment_settings()->context()->GetServiceWorkerRegistration(
      service_worker_object_
          ? service_worker_object_->containing_service_worker_registration()
          : nullptr);
}

scoped_refptr<ServiceWorker> ServiceWorkerGlobalScope::service_worker() const {
  // Algorithm for service_worker():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-global-scope-serviceworker

  // The serviceWorker getter steps are to return the result of getting the
  // service worker object that represents this's service worker in this's
  // relevant settings object.
  DCHECK(service_worker_object_);
  return environment_settings()->context()->GetServiceWorker(
      service_worker_object_);
}


script::HandlePromiseVoid ServiceWorkerGlobalScope::SkipWaiting() {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerGlobalScope::SkipWaiting()");
  DCHECK_EQ(base::MessageLoop::current(),
            environment_settings()->context()->message_loop());
  // Algorithm for skipWaiting():
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dom-serviceworkerglobalscope-skipwaiting
  // 1. Let promise be a new promise.
  auto promise = environment_settings()
                     ->context()
                     ->global_environment()
                     ->script_value_factory()
                     ->CreateBasicPromise<void>();
  std::unique_ptr<script::ValuePromiseVoid::Reference> promise_reference(
      new script::ValuePromiseVoid::Reference(this, promise));

  // 2. Run the following substeps in parallel:
  worker::ServiceWorkerJobs* jobs =
      environment_settings()->context()->service_worker_jobs();
  jobs->message_loop()->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ServiceWorkerJobs::SkipWaitingSubSteps,
                     base::Unretained(jobs),
                     base::Unretained(environment_settings()->context()),
                     base::Unretained(service_worker_object_.get()),
                     std::move(promise_reference)));
  // 3. Return promise.
  return promise;
}

void ServiceWorkerGlobalScope::StartFetch(
    const GURL& url,
    base::OnceCallback<void(std::unique_ptr<std::string>)> callback,
    base::OnceCallback<void(const net::LoadTimingInfo&)>
        report_load_timing_info,
    base::OnceClosure fallback) {
  if (base::MessageLoop::current() !=
      environment_settings()->context()->message_loop()) {
    environment_settings()->context()->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceWorkerGlobalScope::StartFetch,
                       base::Unretained(this), url, std::move(callback),
                       std::move(report_load_timing_info),
                       std::move(fallback)));
    return;
  }
  if (!service_worker()) {
    std::move(fallback).Run();
    return;
  }
  // TODO: handle the following steps in
  //       https://www.w3.org/TR/2022/CRD-service-workers-20220712/#handle-fetch.
  // 22. If activeWorker’s state is "activating", wait for activeWorker’s state
  //     to become "activated".
  // 23. If the result of running the Run Service Worker algorithm with
  //     activeWorker is failure, then set handleFetchFailed to true.

  auto* global_environment = get_global_environment(environment_settings());
  auto* isolate = global_environment->isolate();
  script::v8c::EntryScope entry_scope(isolate);
  auto request = web::cache_utils::CreateRequest(isolate, url.spec());
  if (!request) {
    std::move(fallback).Run();
    return;
  }

  FetchEventInit event_init;
  event_init.set_request(
      web::cache_utils::FromV8Value(isolate, request.value()).GetScriptValue());
  scoped_refptr<FetchEvent> fetch_event =
      new FetchEvent(environment_settings(), base::Tokens::fetch(), event_init,
                     std::move(callback), std::move(report_load_timing_info));
  // 24. Create and dispatch event.
  DispatchEvent(fetch_event);
  // TODO: implement steps 25 and 26.
  if (!fetch_event->respond_with_called()) {
    std::move(fallback).Run();
  }
}

}  // namespace worker
}  // namespace cobalt
