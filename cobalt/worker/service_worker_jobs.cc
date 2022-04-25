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
#include "base/trace_event/trace_event.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

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
}  // namespace

ServiceWorkerJobs::ServiceWorkerJobs(network::NetworkModule* network_module,
                                     base::MessageLoop* message_loop)
    : network_module_(network_module), message_loop_(message_loop) {}

ServiceWorkerJobs::~ServiceWorkerJobs() {}

void ServiceWorkerJobs::StartRegister(
    const base::Optional<GURL>& maybe_scope_url,
    const GURL& script_url_with_fragment,
    const script::Handle<script::Promise<void>>& promise,
    web::EnvironmentSettings* client, const WorkerType& type,
    const ServiceWorkerUpdateViaCache& update_via_cache) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::StartRegister()");
  NOTIMPLEMENTED();
  // Algorithm for "Start Register":
  //   https://w3c.github.io/ServiceWorker/#start-register-algorithm
  // 1. If scriptURL is failure, reject promise with a TypeError and abort these
  // steps.
  if (script_url_with_fragment.is_empty()) {
    promise->Reject(script::kTypeError);
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
    promise->Reject(script::kTypeError);
    return;
  }

  // 4. If any of the strings in scriptURL’s path contains either ASCII
  // case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise with
  // a TypeError and abort these steps.
  if (PathContainsEscapedSlash(script_url)) {
    promise->Reject(script::kTypeError);
    return;
  }

  // 5. If scopeURL is null, set scopeURL to the result of parsing the string
  // "./" with scriptURL.
  GURL scope_url = maybe_scope_url.value_or(script_url.Resolve("./"));

  // 6. If scopeURL is failure, reject promise with a TypeError and abort these
  // steps.
  if (scope_url.is_empty()) {
    promise->Reject(script::kTypeError);
    return;
  }

  // 7. Set scopeURL’s fragment to null.
  scope_url = scope_url.ReplaceComponents(replacements);
  DCHECK(!scope_url.has_ref() || scope_url.ref().empty());
  DCHECK(!scope_url.is_empty());

  // 8. If scopeURL’s scheme is not one of "http" and "https", reject promise
  // with a TypeError and abort these steps.
  if (!scope_url.SchemeIsHTTPOrHTTPS()) {
    promise->Reject(script::kTypeError);
    return;
  }

  // 9. If any of the strings in scopeURL’s path contains either ASCII
  // case-insensitive "%2f" or ASCII case-insensitive "%5c", reject promise with
  // a TypeError and abort these steps.
  if (PathContainsEscapedSlash(scope_url)) {
    promise->Reject(script::kTypeError);
    return;
  }

  // 10. Let storage key be the result of running obtain a storage key given
  // client.

  // 11. Let job be the result of running Create Job with register, storage key,
  // scopeURL, scriptURL, promise, and client.
  std::unique_ptr<Job> job =
      CreateJob(kRegister, scope_url, script_url, promise, client);

  // 12. Set job’s worker type to workerType.
  // Cobalt only supports 'classic' worker type.

  // 13. Set job’s update via cache mode to updateViaCache.
  job->update_via_cache = kServiceWorkerUpdateViaCacheAll;

  // 14. Set job’s referrer to referrer.
  // This is the same value as set in CreateJob().

  // 15. Invoke Schedule Job with job.
  ScheduleJob(std::move(job));
}

void ServiceWorkerJobs::MatchServiceWorkerRegistration(const GURL& client_url) {
  TRACE_EVENT0("cobalt::worker",
               "ServiceWorkerJobs::MatchServiceWorkerRegistration()");
  // TODO this need a callback to return the matching registration.
  NOTIMPLEMENTED();
}

std::unique_ptr<ServiceWorkerJobs::Job> ServiceWorkerJobs::CreateJob(
    JobType type, const GURL& scope_url, const GURL& script_url,
    const script::Handle<script::Promise<void>>& promise,
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
  std::unique_ptr<Job> job(
      new Job(kRegister, scope_url, script_url, promise, client));
  // 8. If client is not null, set job’s referrer to client’s creation URL.
  if (client) {
    job->referrer = client->creation_url();
  }
  // 9. Return job.
  return job;
}

void ServiceWorkerJobs::ScheduleJob(std::unique_ptr<Job> job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::ScheduleJob()");
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
    Job* last_job = job_queue->LastItem();

    // 6.2. If job is equivalent to lastJob and lastJob’s job promise has not
    // settled, append job to lastJob’s list of equivalent jobs.
    {
      base::AutoLock lock(last_job->equivalent_jobs_promise_mutex);
      if (EquivalentJobs(job.get(), last_job) &&
          last_job->promise->State() == script::PromiseState::kPending) {
        last_job->equivalent_jobs.push(std::move(job));
      }
    }

    // 6.3. Else, set job’s containing job queue to jobQueue, and enqueue job to
    // jobQueue.
    job->containing_job_queue = job_queue;
    job_queue->Enqueue(std::move(job));
  }
}

bool ServiceWorkerJobs::EquivalentJobs(Job* one, Job* two) {
  // Algorithm for 'Two jobs are equivalent'
  //   https://w3c.github.io/ServiceWorker/#dfn-job-equivalent
  DCHECK(one);
  DCHECK(two);
  if (!one || !two) {
    return false;
  }

  // when their job type is the same and:
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
  return (one->type != kUnregister) && (one->scope_url == two->scope_url);
}

void ServiceWorkerJobs::RunJob(JobQueue* job_queue) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::RunJob()");
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

  // 2.1 Let job be the first item in jobQueue.
  Job* job = job_queue->FirstItem();

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
  // Algorithm for "Register"
  //   https://w3c.github.io/ServiceWorker/#register-algorithm
}

void ServiceWorkerJobs::Update(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::Update()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for "Update"
  //   https://w3c.github.io/ServiceWorker/#update-algorithm
}

void ServiceWorkerJobs::Unregister(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::Unregister()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for "Unregister"
  //   https://w3c.github.io/ServiceWorker/#unregister-algorithm
}

}  // namespace worker
}  // namespace cobalt
