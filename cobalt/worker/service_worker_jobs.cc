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

#include "cobalt/worker/service_worker_jobs.h"

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/context.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "net/base/url_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

namespace {
bool PathContainsEscapedSlash(const GURL& url) {
  const std::string path = url.path();
  return (path.find("%2f") != std::string::npos ||
          path.find("%2F") != std::string::npos ||
          path.find("%5c") != std::string::npos ||
          path.find("%5C") != std::string::npos);
}

bool IsOriginPotentiallyTrustworthy(const GURL& url) {
  // Algorithm for 'potentially trustworthy origin'.
  //   https://w3c.github.io/webappsec-secure-contexts/#potentially-trustworthy-origin

  const url::Origin origin(url::Origin::Create(url));
  // 1. If origin is an opaque origin, return "Not Trustworthy".
  if (origin.unique()) return false;

  // 2. Assert: origin is a tuple origin.
  DCHECK(!origin.unique());
  DCHECK(url.is_valid());

  // 3. If origin’s scheme is either "https" or "wss", return "Potentially
  // Trustworthy".
  if (url.SchemeIsCryptographic()) return true;

  // 4. If origin’s host matches one of the CIDR notations 127.0.0.0/8 or
  // ::1/128 [RFC4632], return "Potentially Trustworthy".
  if (net::IsLocalhost(url)) return true;

  // 5. If the user agent conforms to the name resolution rules in
  // [let-localhost-be-localhost] and one of the following is true:
  //    origin’s host is "localhost" or "localhost."
  //    origin’s host ends with ".localhost" or ".localhost."
  // then return "Potentially Trustworthy".
  // Covered by implementation of step 4.

  // 6. If origin’s scheme is "file", return "Potentially Trustworthy".
  if (url.SchemeIsFile()) return true;

  // 7. If origin’s scheme component is one which the user agent considers to be
  // authenticated, return "Potentially Trustworthy".
  if (url.SchemeIs("h5vcc-embedded")) return true;

  // 8. If origin has been configured as a trustworthy origin, return
  // "Potentially Trustworthy".

  // 9. Return "Not Trustworthy".
  return false;
}

// Returns the serialized URL excluding the fragment.
std::string SerializeExcludingFragment(const GURL& url) {
  url::Replacements<char> replacements;
  replacements.ClearRef();
  GURL no_fragment_url = url.ReplaceComponents(replacements);
  DCHECK(!no_fragment_url.has_ref() || no_fragment_url.ref().empty());
  DCHECK(!no_fragment_url.is_empty());
  return no_fragment_url.spec();
}
}  // namespace

ServiceWorkerJobs::ServiceWorkerJobs(network::NetworkModule* network_module,
                                     base::MessageLoop* message_loop)
    : network_module_(network_module), message_loop_(message_loop) {}

ServiceWorkerJobs::~ServiceWorkerJobs() {}

void ServiceWorkerJobs::StartRegister(
    const base::Optional<GURL>& maybe_scope_url,
    const GURL& script_url_with_fragment,
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference,
    web::EnvironmentSettings* client, const WorkerType& type,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::StartRegister()");
  // Algorithm for "Start Register":
  //   https://w3c.github.io/ServiceWorker/#start-register-algorithm
  // 1. If scriptURL is failure, reject promise with a TypeError and abort these
  // steps.
  if (script_url_with_fragment.is_empty()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 2. Set scriptURL’s fragment to null.
  url::Replacements<char> replacements;
  replacements.ClearRef();
  GURL script_url = script_url_with_fragment.ReplaceComponents(replacements);
  DCHECK(!script_url.has_ref() || script_url.ref().empty());
  DCHECK(!script_url.is_empty());

  // 3. If scriptURL’s scheme is not one of "http" and "https", reject promise
  // with a TypeError and abort these steps.
  if (!script_url.SchemeIsHTTPOrHTTPS()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 4. If any of the strings in scriptURL’s path contains either ASCII
  // case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise with
  // a TypeError and abort these steps.
  if (PathContainsEscapedSlash(script_url)) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 5. If scopeURL is null, set scopeURL to the result of parsing the string
  // "./" with scriptURL.
  GURL scope_url = maybe_scope_url.value_or(script_url.Resolve("./"));

  // 6. If scopeURL is failure, reject promise with a TypeError and abort these
  // steps.
  if (scope_url.is_empty()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 7. Set scopeURL’s fragment to null.
  scope_url = scope_url.ReplaceComponents(replacements);
  DCHECK(!scope_url.has_ref() || scope_url.ref().empty());
  DCHECK(!scope_url.is_empty());

  // 8. If scopeURL’s scheme is not one of "http" and "https", reject promise
  // with a TypeError and abort these steps.
  if (!scope_url.SchemeIsHTTPOrHTTPS()) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 9. If any of the strings in scopeURL’s path contains either ASCII
  // case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise with
  // a TypeError and abort these steps.
  if (PathContainsEscapedSlash(scope_url)) {
    promise_reference->value().Reject(script::kTypeError);
    return;
  }

  // 10. Let storage key be the result of running obtain a storage key given
  // client.
  url::Origin storage_key = url::Origin::Create(client->creation_url());

  // 11. Let job be the result of running Create Job with register, storage key,
  // scopeURL, scriptURL, promise, and client.
  std::unique_ptr<Job> job =
      CreateJob(kRegister, storage_key, scope_url, script_url,
                JobPromiseType::Create(std::move(promise_reference)), client);
  DCHECK(!promise_reference);

  // 12. Set job’s worker type to workerType.
  // Cobalt only supports 'classic' worker type.

  // 13. Set job’s update via cache mode to updateViaCache.
  job->update_via_cache = update_via_cache;

  // 14. Set job’s referrer to referrer.
  // This is the same value as set in CreateJob().

  // 15. Invoke Schedule Job with job.
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerJobs::ScheduleJob,
                            base::Unretained(this), base::Passed(&job)));
  DCHECK(!job.get());
}

void ServiceWorkerJobs::MatchServiceWorkerRegistration(const GURL& client_url) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerJobs::MatchServiceWorkerRegistration()");
  // TODO this need a callback to return the matching registration.
  NOTIMPLEMENTED();
}

void ServiceWorkerJobs::PromiseErrorData::Reject(
    std::unique_ptr<JobPromiseType> promise) const {
  if (message_type_ != script::kNoError) {
    promise->Reject(GetSimpleExceptionType(message_type_));
  } else {
    promise->Reject(new dom::DOMException(exception_code_, message_));
  }
}

std::unique_ptr<ServiceWorkerJobs::Job> ServiceWorkerJobs::CreateJob(
    JobType type, const url::Origin& storage_key, const GURL& scope_url,
    const GURL& script_url, std::unique_ptr<JobPromiseType> promise,
    web::EnvironmentSettings* client) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::CreateJob()");
  // Algorithm for "Create Job":
  //   https://w3c.github.io/ServiceWorker/#create-job
  // 1. Let job be a new job.
  // 2. Set job’s job type to jobType.
  // 3. Set job’s storage key to storage key.
  // 4. Set job’s scope url to scopeURL.
  // 5. Set job’s script url to scriptURL.
  // 6. Set job’s job promise to promise.
  // 7. Set job’s client to client.
  std::unique_ptr<Job> job(new Job(kRegister, storage_key, scope_url,
                                   script_url, std::move(promise), client));
  // 8. If client is not null, set job’s referrer to client’s creation URL.
  if (client) {
    job->referrer = client->creation_url();
  }
  // 9. Return job.
  return job;
}

void ServiceWorkerJobs::ScheduleJob(std::unique_ptr<Job> job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::ScheduleJob()");
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  DCHECK(job);
  // Algorithm for "Schedule Job":
  //   https://w3c.github.io/ServiceWorker/#schedule-job
  // 1. Let jobQueue be null.

  // 2. Let jobScope be job’s scope url, serialized.
  std::string job_scope = job->scope_url.spec();

  // 3. If scope to job queue map[jobScope] does not exist, set scope to job
  // queue map[jobScope] to a new job queue.
  if (job_queue_map_.find(job_scope) == job_queue_map_.end()) {
    auto insertion = job_queue_map_.emplace(
        job_scope, std::unique_ptr<JobQueue>(new JobQueue()));
    DCHECK(insertion.second);
  }

  // 4. Set jobQueue to scope to job queue map[jobScope].
  DCHECK(job_queue_map_.find(job_scope) != job_queue_map_.end());
  JobQueue* job_queue = job_queue_map_.find(job_scope)->second.get();

  // 5. If jobQueue is empty, then:
  if (job_queue->empty()) {
    // 5.1. Set job’s containing job queue to jobQueue, and enqueue job to
    // jobQueue.
    job->containing_job_queue = job_queue;
    job_queue->Enqueue(std::move(job));

    // 5.2. Invoke Run Job with jobQueue.
    RunJob(job_queue);

  } else {
    // 6. Else:
    // 6.1. Let lastJob be the element at the back of jobQueue.
    {
      auto last_item = job_queue->LastItem();
      Job* last_job = last_item.first;

      // 6.2. If job is equivalent to lastJob and lastJob’s job promise has not
      // settled, append job to lastJob’s list of equivalent jobs.
      DCHECK(last_job);
      base::AutoLock lock(last_job->equivalent_jobs_promise_mutex);
      if (EquivalentJobs(job.get(), last_job) && last_job->promise &&
          last_job->promise->State() == script::PromiseState::kPending) {
        last_job->equivalent_jobs.push_back(std::move(job));
        return;
      }
    }

    // 6.3. Else, set job’s containing job queue to jobQueue, and enqueue job to
    // jobQueue.
    job->containing_job_queue = job_queue;
    job_queue->Enqueue(std::move(job));
  }
  DCHECK(!job);
}

bool ServiceWorkerJobs::EquivalentJobs(Job* one, Job* two) {
  // Algorithm for 'Two jobs are equivalent'
  //   https://w3c.github.io/ServiceWorker/#dfn-job-equivalent
  DCHECK(one);
  DCHECK(two);
  if (!one || !two) {
    return false;
  }

  // Two jobs are equivalent when their job type is the same and:
  if (one->type != two->type) {
    return false;
  }

  // For register and update jobs, their scope url, script url, worker type, and
  // update via cache mode are the same.
  if ((one->type == kRegister || one->type == kUpdate) &&
      (one->scope_url == two->scope_url) &&
      (one->script_url == two->script_url) &&
      (one->update_via_cache == two->update_via_cache)) {
    return true;
  }

  // For unregister jobs, their scope url is the same.
  return (one->type == kUnregister) && (one->scope_url == two->scope_url);
}

void ServiceWorkerJobs::RunJob(JobQueue* job_queue) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::RunJob()");
  DCHECK_EQ(base::MessageLoop::current(), message_loop());
  // Algorithm for "Run Job":
  //   https://w3c.github.io/ServiceWorker/#run-job-algorithm

  // 1. Assert: jobQueue is not empty.
  DCHECK(job_queue && !job_queue->empty());
  if (!job_queue || job_queue->empty()) {
    return;
  }

  // 2. Queue a task to run these steps:
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ServiceWorkerJobs::RunJobTask,
                            base::Unretained(this), job_queue));
}

void ServiceWorkerJobs::RunJobTask(JobQueue* job_queue) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::RunJobTask()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Task for "Run Job" to run in the service worker thread.
  //   https://w3c.github.io/ServiceWorker/#run-job-algorithm
  DCHECK(job_queue);
  if (!job_queue) return;
  DCHECK(!job_queue->empty());

  // 2.1 Let job be the first item in jobQueue.
  Job* job = job_queue->FirstItem();

  DCHECK(job);
  switch (job->type) {
    // 2.2 If job’s job type is register, run Register with job in parallel.
    case kRegister:
      Register(job);
      break;

    // 2.3 Else if job’s job type is update, run Update with job in parallel.
    case kUpdate:
      Update(job);
      break;

    // 2.4 Else if job’s job type is unregister, run Unregister with job in
    // parallel.
    case kUnregister:
      Unregister(job);
      break;
    default:
      NOTREACHED();
  }
}

void ServiceWorkerJobs::Register(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::Register()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(job);
  // Algorithm for "Register"
  //   https://w3c.github.io/ServiceWorker/#register-algorithm

  // 1. If the result of running potentially trustworthy origin with the origin
  // of job’s script url as the argument is Not Trusted, then:
  if (!IsOriginPotentiallyTrustworthy(job->script_url)) {
    // 1.1. Invoke Reject Job Promise with job and "SecurityError" DOMException.
    RejectJobPromise(
        job, PromiseErrorData(
                 dom::DOMException::kSecurityErr,
                 "Service Worker Register failed: Script URL is Not Trusted."));
    // 1.2. Invoke Finish Job with job and abort these steps.
    FinishJob(job);
    return;
  }

  // 2. If job’s script url's origin and job’s referrer's origin are not same
  // origin, then:
  const url::Origin job_script_origin(url::Origin::Create(job->script_url));
  const url::Origin job_referrer_origin(url::Origin::Create(job->referrer));
  if (!job_script_origin.IsSameOriginWith(job_referrer_origin)) {
    // 2.1. Invoke Reject Job Promise with job and "SecurityError" DOMException.
    RejectJobPromise(
        job, PromiseErrorData(
                 dom::DOMException::kSecurityErr,
                 "Service Worker Register failed: Script URL and referrer "
                 "origin are not the same."));
    // 2.2. Invoke Finish Job with job and abort these steps.
    FinishJob(job);
    return;
  }

  // 3. If job’s scope url's origin and job’s referrer's origin are not same
  // origin, then:
  const url::Origin job_scope_origin(url::Origin::Create(job->scope_url));
  if (!job_scope_origin.IsSameOriginWith(job_referrer_origin)) {
    // 3.1. Invoke Reject Job Promise with job and "SecurityError" DOMException.
    RejectJobPromise(
        job, PromiseErrorData(
                 dom::DOMException::kSecurityErr,
                 "Service Worker Register failed: Scope URL and referrer "
                 "origin are not the same."));

    // 3.2. Invoke Finish Job with job and abort these steps.
    FinishJob(job);
    return;
  }

  // 4. Let registration be the result of running Get Registration given job’s
  // storage key and job’s scope url.
  ServiceWorkerRegistrationObject* registration =
      GetRegistration(job->storage_key, job->scope_url);

  // 5. If registration is not null, then:
  if (registration) {
    // 5.1 Let newestWorker be the result of running the Get Newest Worker
    // algorithm passing registration as the argument.
    ServiceWorker* newest_worker = GetNewestWorker(registration);

    // 5.2 If newestWorker is not null, job’s script url equals newestWorker’s
    // script url, job’s worker type equals newestWorker’s type, and job’s
    // update via cache mode's value equals registration’s update via cache
    // mode, then:
    if (newest_worker && job->script_url == newest_worker->script_url()) {
      // 5.2.1 Invoke Resolve Job Promise with job and registration.
      ResolveJobPromise(job, registration);

      // 5.2.2 Invoke Finish Job with job and abort these steps.
      FinishJob(job);
      return;
    }
  } else {
    // 6. Else:

    // 6.1 Invoke Set Registration algorithm with job’s storage key, job’s scope
    // url, and job’s update via cache mode.
    registration = SetRegistration(job->storage_key, job->scope_url,
                                   job->update_via_cache);
  }

  // 7. Invoke Update algorithm passing job as the argument.
  Update(job);
}

void ServiceWorkerJobs::Update(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::Update()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(job);
  // Algorithm for "Update"
  //   https://w3c.github.io/ServiceWorker/#update-algorithm

  // 1. Let registration be the result of running Get Registration given job’s
  // storage key and job’s scope url.
  ServiceWorkerRegistrationObject* registration =
      GetRegistration(job->storage_key, job->scope_url);

  // 2. If registration is null, then:
  if (!registration) {
    // 2.1. Invoke Reject Job Promise with job and TypeError.
    RejectJobPromise(job, PromiseErrorData(script::kSimpleTypeError));

    // 2.2. Invoke Finish Job with job and abort these steps.
    FinishJob(job);
    return;
  }
  ResolveJobPromise(job, registration);
  NOTIMPLEMENTED();
}

void ServiceWorkerJobs::Unregister(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::Unregister()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for "Unregister"
  //   https://w3c.github.io/ServiceWorker/#unregister-algorithm
  NOTIMPLEMENTED();
}

void ServiceWorkerJobs::RejectJobPromise(Job* job,
                                         const PromiseErrorData& error_data) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::RejectJobPromise()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for "Reject Job Promise"
  //   https://w3c.github.io/ServiceWorker/#reject-job-promise
  // 1. If job’s client is not null, queue a task, on job’s client's responsible
  // event loop using the DOM manipulation task source, to reject job’s job
  // promise with a new exception with errorData and a user agent-defined
  // message, in job’s client's Realm.

  auto reject_task = [](std::unique_ptr<JobPromiseType> promise,
                        const PromiseErrorData& error_data) {
    error_data.Reject(std::move(promise));
  };

  if (job->client) {
    base::AutoLock lock(job->equivalent_jobs_promise_mutex);
    job->client->context()->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(reject_task, base::Passed(&job->promise), error_data));
    // Ensure that the promise is cleared, so that equivalent jobs won't get
    // added from this point on.
    CHECK(!job->promise);
  }
  // 2. For each equivalentJob in job’s list of equivalent jobs:
  for (auto& equivalent_job : job->equivalent_jobs) {
    // Equivalent jobs should never have equivalent jobs of their own.
    DCHECK(equivalent_job->equivalent_jobs.empty());

    // 2.1. If equivalentJob’s client is null, continue.
    if (equivalent_job->client) {
      // 2.2. Queue a task, on equivalentJob’s client's responsible event loop
      // using the DOM manipulation task source, to reject equivalentJob’s job
      // promise with a new exception with errorData and a user agent-defined
      // message, in equivalentJob’s client's Realm.
      equivalent_job->client->context()
          ->message_loop()
          ->task_runner()
          ->PostTask(FROM_HERE,
                     base::BindOnce(reject_task,
                                    base::Passed(&equivalent_job->promise),
                                    error_data));
      // Check that the promise is cleared.
      CHECK(!equivalent_job->promise);
    }
  }
  job->equivalent_jobs.clear();
}

void ServiceWorkerJobs::ResolveJobPromise(
    Job* job, ServiceWorkerRegistrationObject* value) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::ResolveJobPromise()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(job);
  DCHECK(value);
  // Algorithm for "Resolve Job Promise".
  //   https://w3c.github.io/ServiceWorker/#resolve-job-promise-algorithm

  // 1. If job’s client is not null, queue a task, on job’s client's responsible
  // event loop using the DOM manipulation task source, to run the following
  // substeps:
  if (job->client) {
    base::AutoLock lock(job->equivalent_jobs_promise_mutex);
    job->client->context()->message_loop()->task_runner()->PostTask(
        FROM_HERE, base::Bind(&ServiceWorkerJobs::ResolveJobPromiseTask,
                              base::Unretained(this), job->type,
                              base::Passed(&job->promise), job->client, value));
    // Ensure that the promise is cleared, so that equivalent jobs won't get
    // added from this point on.
    CHECK(!job->promise);
  }

  // 2. For each equivalentJob in job’s list of equivalent jobs:
  for (auto& equivalent_job : job->equivalent_jobs) {
    // Equivalent jobs should never have equivalent jobs of their own.
    DCHECK(equivalent_job->equivalent_jobs.empty());

    // 2.1. If equivalentJob’s client is null, continue to the next iteration of
    // the loop.
    if (equivalent_job->client) {
      // 2.2. Queue a task, on equivalentJob’s client's responsible event loop
      // using the DOM manipulation task source, to run the following substeps:
      equivalent_job->client->context()
          ->message_loop()
          ->task_runner()
          ->PostTask(FROM_HERE,
                     base::Bind(&ServiceWorkerJobs::ResolveJobPromiseTask,
                                base::Unretained(this), equivalent_job->type,
                                base::Passed(&equivalent_job->promise),
                                equivalent_job->client, value));
      // Check that the promise is cleared.
      CHECK(!equivalent_job->promise);
    }
  }
  job->equivalent_jobs.clear();
}

void ServiceWorkerJobs::ResolveJobPromiseTask(
    JobType type, std::unique_ptr<JobPromiseType> promise,
    web::EnvironmentSettings* client, ServiceWorkerRegistrationObject* value) {
  DCHECK_NE(message_loop(), base::MessageLoop::current());
  DCHECK_EQ(base::MessageLoop::current(), client->context()->message_loop());
  // 1.1./2.2.1. Let convertedValue be null.
  // 1.2./2.2.2. If job’s job type is either register or update, set
  // convertedValue to the result of getting the service worker registration
  // object that represents value in job’s client.
  if (type == kRegister || type == kUpdate) {
    auto converted_value =
        client->context()->GetServiceWorkerRegistration(value);
    // 1.4./2.2.4.  Resolve job’s job promise with convertedValue.
    promise->Resolve(converted_value);
  } else {
    DCHECK_EQ(type, kUnregister);
    // 1.3./2.2.3. Else, set convertedValue to value, in job’s client's Realm.
    bool converted_value = value != nullptr;
    // 1.4./2.2.4.  Resolve job’s job promise with convertedValue.
    promise->Resolve(converted_value);
  }
}

// https://w3c.github.io/ServiceWorker/#finish-job-algorithm
void ServiceWorkerJobs::FinishJob(Job* job) {
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // 1. Let jobQueue be job’s containing job queue.
  JobQueue* job_queue = job->containing_job_queue;

  // 2. Assert: the first item in jobQueue is job.
  DCHECK_EQ(job_queue->FirstItem(), job);

  // 3. Dequeue from jobQueue.
  job_queue->Dequeue();

  // 4. If jobQueue is not empty, invoke Run Job with jobQueue.
  if (!job_queue->empty()) {
    RunJob(job_queue);
  }
}

ServiceWorkerRegistrationObject* ServiceWorkerJobs::GetRegistration(
    const url::Origin& storage_key, const GURL& scope) {
  // Algorithm for 'Get Registration':
  //   https://w3c.github.io/ServiceWorker/#get-registration-algorithm
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // 1. Run the following steps atomically.
  base::AutoLock lock(registration_map_mutex_);

  // 2. Let scopeString be the empty string.
  std::string scope_string;

  // 3. If scope is not null, set scopeString to serialized scope with the
  // exclude fragment flag set.
  if (!scope.is_empty()) {
    scope_string = SerializeExcludingFragment(scope);
  }

  RegistrationKey registration_key(storage_key, scope_string);
  // 4. For each (entry storage key, entry scope) → registration of registration
  // map:
  for (const auto& entry : registration_map_) {
    // 4.1. If storage key equals entry storage key and scopeString matches
    // entry scope, then return registration.
    if (entry.first == registration_key) {
      return entry.second.get();
    }
  }

  // 5. Return null.
  return nullptr;
}

ServiceWorkerRegistrationObject* ServiceWorkerJobs::SetRegistration(
    const url::Origin& storage_key, const GURL& scope,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  // Algorithm for 'Set Registration':
  //   https://w3c.github.io/ServiceWorker/#set-registration-algorithm
  DCHECK_EQ(message_loop(), base::MessageLoop::current());

  // 1. Run the following steps atomically.
  base::AutoLock lock(registration_map_mutex_);

  // 2. Let scopeString be serialized scope with the exclude fragment flag set.
  std::string scope_string = SerializeExcludingFragment(scope);

  // 3. Let registration be a new service worker registration whose storage key
  // is set to storage key, scope url is set to scope, and update via cache mode
  // is set to updateViaCache.
  ServiceWorkerRegistrationObject* registration(
      new ServiceWorkerRegistrationObject(storage_key, scope,
                                          update_via_cache));

  // 4. Set registration map[(storage key, scopeString)] to registration.
  RegistrationKey registration_key(storage_key, scope_string);
  registration_map_.insert(std::make_pair(
      registration_key,
      std::unique_ptr<ServiceWorkerRegistrationObject>(registration)));

  // 5. Return registration.
  return registration;
}

// https://w3c.github.io/ServiceWorker/#get-newest-worker
ServiceWorker* ServiceWorkerJobs::GetNewestWorker(
    ServiceWorkerRegistrationObject* registration) {
  NOTIMPLEMENTED();
  return nullptr;
}

ServiceWorkerJobs::JobPromiseType::JobPromiseType(
    std::unique_ptr<script::ValuePromiseBool::Reference> promise_reference)
    : promise_bool_reference_(std::move(promise_reference)) {}
ServiceWorkerJobs::JobPromiseType::JobPromiseType(
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference)
    : promise_wrappable_reference_(std::move(promise_reference)) {}

void ServiceWorkerJobs::JobPromiseType::Resolve(const bool result) {
  DCHECK(promise_bool_reference_);
  promise_bool_reference_->value().Resolve(result);
}

void ServiceWorkerJobs::JobPromiseType::Resolve(
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  DCHECK(promise_wrappable_reference_);
  promise_wrappable_reference_->value().Resolve(result);
}

void ServiceWorkerJobs::JobPromiseType::Reject(
    script::SimpleExceptionType exception) {
  if (promise_bool_reference_) {
    promise_bool_reference_->value().Reject(exception);
    return;
  }
  if (promise_wrappable_reference_) {
    promise_wrappable_reference_->value().Reject(exception);
    return;
  }
  NOTREACHED();
}

void ServiceWorkerJobs::JobPromiseType::Reject(
    const scoped_refptr<script::ScriptException>& result) {
  if (promise_bool_reference_) {
    promise_bool_reference_->value().Reject(result);
    return;
  }
  if (promise_wrappable_reference_) {
    promise_wrappable_reference_->value().Reject(result);
    return;
  }
  NOTREACHED();
}

script::PromiseState ServiceWorkerJobs::JobPromiseType::State() {
  if (promise_bool_reference_) {
    return promise_bool_reference_->value().State();
  }
  if (promise_wrappable_reference_) {
    return promise_wrappable_reference_->value().State();
  }
  NOTREACHED();
  return script::PromiseState::kPending;
}

}  // namespace worker
}  // namespace cobalt
