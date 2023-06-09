// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/worker/service_worker_abstract_jobs.h"

#include <list>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/base/tokens.h"
#include "cobalt/base/type_id.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_exception.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/web/event.h"
#include "cobalt/web/window_or_worker_global_scope.h"
#include "cobalt/worker/client.h"
#include "cobalt/worker/client_query_options.h"
#include "cobalt/worker/client_type.h"
#include "cobalt/worker/extendable_event.h"
#include "cobalt/worker/extendable_message_event.h"
#include "cobalt/worker/frame_type.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_consts.h"
#include "cobalt/worker/service_worker_container.h"
#include "cobalt/worker/service_worker_global_scope.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/window_client.h"
#include "cobalt/worker/worker_type.h"
#include "net/base/mime_util.h"
#include "net/base/url_util.h"
#include "starboard/common/atomic.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

ServiceWorkerAbstractJobs::ServiceWorkerAbstractJobs(
    base::MessageLoop* message_loop)
    : message_loop_(message_loop) {}

std::unique_ptr<ServiceWorkerAbstractJobs::Job>
ServiceWorkerAbstractJobs::CreateJob(JobType type,
                                     const url::Origin& storage_key,
                                     const GURL& scope_url,
                                     const GURL& script_url,
                                     std::unique_ptr<JobPromiseType> promise,
                                     web::Context* client) {
  TRACE_EVENT2("cobalt::worker", "ServiceWorkerAbstractJobs::CreateJob()",
               "type", type, "script_url", script_url.spec());
  // Algorithm for Create Job:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-job
  // 1. Let job be a new job.
  // 2. Set job’s job type to jobType.
  // 3. Set job’s storage key to storage key.
  // 4. Set job’s scope url to scopeURL.
  // 5. Set job’s script url to scriptURL.
  // 6. Set job’s job promise to promise.
  // 7. Set job’s client to client.
  std::unique_ptr<Job> job(new Job(type, storage_key, scope_url, script_url,
                                   client, std::move(promise)));
  // 8. If client is not null, set job’s referrer to client’s creation URL.
  if (client) {
    job->referrer = client->environment_settings()->creation_url();
  }
  // 9. Return job.
  return job;
}

void ServiceWorkerAbstractJobs::ScheduleJob(std::unique_ptr<Job> job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerAbstractJobs::ScheduleJob()");
  DCHECK(job);

  if (base::MessageLoop::current() != message_loop()) {
    DCHECK(message_loop());
    message_loop()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&ServiceWorkerAbstractJobs::ScheduleJob,
                                  base::Unretained(this), std::move(job)));
    return;
  }
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Schedule Job:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#schedule-job
  // 1. Let jobQueue be null.

  // 2. Let jobScope be job’s scope url, serialized.
  std::string job_scope = job->scope_url.spec();

  // 3. If scope to job queue map[jobScope] does not exist, set scope to job
  // queue map[jobScope] to a new job queue.
  auto job_queue_iterator_for_scope = job_queue_map_.find(job_scope);
  if (job_queue_iterator_for_scope == job_queue_map_.end()) {
    auto insertion = job_queue_map_.emplace(
        job_scope, std::unique_ptr<JobQueue>(new JobQueue()));
    DCHECK(insertion.second);
    job_queue_iterator_for_scope = insertion.first;
  }

  // 4. Set jobQueue to scope to job queue map[jobScope].
  DCHECK(job_queue_iterator_for_scope != job_queue_map_.end());
  JobQueue* job_queue = job_queue_iterator_for_scope->second.get();

  // 5. If jobQueue is empty, then:
  if (job_queue->empty()) {
    // 5.1. Set job’s containing job queue to jobQueue, and enqueue job to
    // jobQueue.
    job->containing_job_queue = job_queue;
    if (!IsWebContextRegistered(job->client)) {
      // Note: The client that requested the job has already exited and isn't
      // able to handle the promise.
      job->containing_job_queue->PrepareJobForClientShutdown(job, job->client);
    }
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
      if (ReturnJobsAreEquivalent(job.get(), last_job) && last_job->promise &&
          last_job->promise->is_pending()) {
        last_job->equivalent_jobs.push_back(std::move(job));
        return;
      }
    }

    // 6.3. Else, set job’s containing job queue to jobQueue, and enqueue job to
    // jobQueue.
    job->containing_job_queue = job_queue;
    if (!IsWebContextRegistered(job->client)) {
      // Note: The client that requested the job has already exited and isn't
      // able to handle the promise.
      job->containing_job_queue->PrepareJobForClientShutdown(job, job->client);
    }
    job_queue->Enqueue(std::move(job));
  }
  DCHECK(!job);
}

bool ServiceWorkerAbstractJobs::ReturnJobsAreEquivalent(Job* one, Job* two) {
  // Algorithm for Two jobs are equivalent:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-job-equivalent
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

void ServiceWorkerAbstractJobs::RunJob(JobQueue* job_queue) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerAbstractJobs::RunJob()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Run Job:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#run-job-algorithm

  // 1. Assert: jobQueue is not empty.
  DCHECK(job_queue && !job_queue->empty());
  if (!job_queue || job_queue->empty()) {
    return;
  }

  // 2. Queue a task to run these steps:
  message_loop()->task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&ServiceWorkerAbstractJobs::RunJobTask,
                                base::Unretained(this), job_queue));
}

ServiceWorkerAbstractJobs::JobPromiseType::JobPromiseType(
    std::unique_ptr<script::ValuePromiseBool::Reference> promise_reference)
    : promise_bool_reference_(std::move(promise_reference)) {}
ServiceWorkerAbstractJobs::JobPromiseType::JobPromiseType(
    std::unique_ptr<script::ValuePromiseWrappable::Reference> promise_reference)
    : promise_wrappable_reference_(std::move(promise_reference)) {}

void ServiceWorkerAbstractJobs::JobPromiseType::Resolve(const bool result) {
  DCHECK(promise_bool_reference_);
  is_pending_.store(false);
  promise_bool_reference_->value().Resolve(result);
}

void ServiceWorkerAbstractJobs::JobPromiseType::Resolve(
    const scoped_refptr<cobalt::script::Wrappable>& result) {
  DCHECK(promise_wrappable_reference_);
  is_pending_.store(false);
  promise_wrappable_reference_->value().Resolve(result);
}

void ServiceWorkerAbstractJobs::JobPromiseType::Reject(
    script::SimpleExceptionType exception) {
  if (promise_bool_reference_) {
    is_pending_.store(false);
    promise_bool_reference_->value().Reject(exception);
    return;
  }
  if (promise_wrappable_reference_) {
    is_pending_.store(false);
    promise_wrappable_reference_->value().Reject(exception);
    return;
  }
  NOTREACHED();
}

void ServiceWorkerAbstractJobs::JobPromiseType::Reject(
    const scoped_refptr<script::ScriptException>& result) {
  if (promise_bool_reference_) {
    is_pending_.store(false);
    promise_bool_reference_->value().Reject(result);
    return;
  }
  if (promise_wrappable_reference_) {
    is_pending_.store(false);
    promise_wrappable_reference_->value().Reject(result);
    return;
  }
  NOTREACHED();
}

void ServiceWorkerAbstractJobs::PromiseErrorData::Reject(
    std::unique_ptr<JobPromiseType> promise) const {
  DCHECK(promise);
  if (message_type_ != script::kNoError) {
    promise->Reject(GetSimpleExceptionType(message_type_));
  } else {
    promise->Reject(new web::DOMException(exception_code_, message_));
  }
}

void ServiceWorkerAbstractJobs::RejectJobPromise(
    Job* job, const PromiseErrorData& error_data) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::RejectJobPromise()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // Algorithm for Reject Job Promise:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#reject-job-promise
  base::AutoLock lock(job->equivalent_jobs_promise_mutex);
  // 1. If job’s client is not null, queue a task, on job’s client's responsible
  //    event loop using the DOM manipulation task source, to reject job’s job
  //    promise with a new exception with errorData and a user agent-defined
  //    message, in job’s client's Realm.
  // 2.1. If equivalentJob’s client is null, continue.
  // 2.2. Queue a task, on equivalentJob’s client's responsible event loop
  //      using the DOM manipulation task source, to reject equivalentJob’s
  //      job promise with a new exception with errorData and a user
  //      agent-defined message, in equivalentJob’s client's Realm.
  if (job->client && job->promise != nullptr) {
    DCHECK(IsWebContextRegistered(job->client));
    job->client->message_loop()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](std::unique_ptr<JobPromiseType> promise,
                          const PromiseErrorData& error_data) {
                         error_data.Reject(std::move(promise));
                       },
                       std::move(job->promise), error_data));
    // Ensure that the promise is cleared, so that equivalent jobs won't get
    // added from this point on.
    CHECK(!job->promise);
  }
  // 2. For each equivalentJob in job’s list of equivalent jobs:
  for (auto& equivalent_job : job->equivalent_jobs) {
    // Recurse for the equivalent jobs.
    RejectJobPromise(equivalent_job.get(), error_data);
  }
  job->equivalent_jobs.clear();
}

void ServiceWorkerAbstractJobs::ResolveJobPromise(
    Job* job, bool value,
    const scoped_refptr<ServiceWorkerRegistrationObject>& registration) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::ResolveJobPromise()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  DCHECK(job);
  // Algorithm for Resolve Job Promise:
  //   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#resolve-job-promise-algorithm
  base::AutoLock lock(job->equivalent_jobs_promise_mutex);
  // 1. If job’s client is not null, queue a task, on job’s client's responsible
  //    event loop using the DOM manipulation task source, to run the following
  //    substeps:
  // 2.1 If equivalentJob’s client is null, continue to the next iteration of
  // the loop.
  if (job->client && job->promise != nullptr) {
    DCHECK(IsWebContextRegistered(job->client));
    job->client->message_loop()->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](JobType type, web::Context* client,
               std::unique_ptr<JobPromiseType> promise, bool value,
               scoped_refptr<ServiceWorkerRegistrationObject> registration) {
              TRACE_EVENT0(
                  "cobalt::worker",
                  "ServiceWorkerJobs::ResolveJobPromise() ResolveTask");
              // 1.1./2.2.1. Let convertedValue be null.
              // 1.2./2.2.2. If job’s job type is either register or update, set
              //             convertedValue to the result of getting the service
              //             worker registration object that represents value in
              //             job’s client.
              if (type == kRegister || type == kUpdate) {
                scoped_refptr<cobalt::script::Wrappable> converted_value =
                    client->GetServiceWorkerRegistration(registration);
                // 1.4./2.2.4. Resolve job’s job promise with convertedValue.
                promise->Resolve(converted_value);
              } else {
                DCHECK_EQ(kUnregister, type);
                // 1.3./2.2.3. Else, set convertedValue to value, in job’s
                // client's
                //             Realm.
                bool converted_value = value;
                // 1.4./2.2.4. Resolve job’s job promise with convertedValue.
                promise->Resolve(converted_value);
              }
            },
            job->type, job->client, std::move(job->promise), value,
            registration));
    // Ensure that the promise is cleared, so that equivalent jobs won't get
    // added from this point on.
    CHECK(!job->promise);
  }

  // 2. For each equivalentJob in job’s list of equivalent jobs:
  for (auto& equivalent_job : job->equivalent_jobs) {
    // Recurse for the equivalent jobs.
    ResolveJobPromise(equivalent_job.get(), value, registration);
  }
  job->equivalent_jobs.clear();
}

// https://www.w3.org/TR/2022/CRD-service-workers-20220712/#finish-job-algorithm
void ServiceWorkerAbstractJobs::FinishJob(Job* job) {
  TRACE_EVENT0("cobalt::worker", "ServiceWorkerJobs::FinishJob()");
  DCHECK_EQ(message_loop(), base::MessageLoop::current());
  // 1. Let jobQueue be job’s containing job queue.
  JobQueue* job_queue = job->containing_job_queue;

  // 2. Assert: the first item in jobQueue is job.
  DCHECK_EQ(job, job_queue->FirstItem());

  // 3. Dequeue from jobQueue.
  job_queue->Dequeue();

  // 4. If jobQueue is not empty, invoke Run Job with jobQueue.
  if (!job_queue->empty()) {
    RunJob(job_queue);
  }
}

}  // namespace worker
}  // namespace cobalt
