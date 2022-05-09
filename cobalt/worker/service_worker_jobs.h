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

#ifndef COBALT_WORKER_SERVICE_WORKER_JOBS_H_
#define COBALT_WORKER_SERVICE_WORKER_JOBS_H_

#include <deque>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/task/sequence_manager/moveable_auto_lock.h"
#include "cobalt/dom/dom_exception.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// Algorithms for Service Worker Jobs.
//   https://w3c.github.io/ServiceWorker/#algorithms
class ServiceWorkerJobs {
 public:
  ServiceWorkerJobs(network::NetworkModule* network_module,
                    base::MessageLoop* message_loop);
  ~ServiceWorkerJobs();

  base::MessageLoop* message_loop() { return message_loop_; }
  network::NetworkModule* network_module() { return network_module_; }

  // https://w3c.github.io/ServiceWorker/#start-register-algorithm
  void StartRegister(const base::Optional<GURL>& scope_url,
                     const GURL& script_url,
                     std::unique_ptr<script::ValuePromiseWrappable::Reference>
                         promise_reference,
                     web::EnvironmentSettings* client, const WorkerType& type,
                     const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://w3c.github.io/ServiceWorker/#scope-match-algorithm
  void MatchServiceWorkerRegistration(const GURL& client_url);

 private:
  // https://w3c.github.io/ServiceWorker/#dfn-job-type
  enum JobType { kRegister, kUpdate, kUnregister };

  class JobQueue;

  // This type handles the different promise variants used in jobs.
  class JobPromiseType {
   public:
    // Constructors for each promise variant that can be held.
    explicit JobPromiseType(
        std::unique_ptr<script::ValuePromiseBool::Reference> promise_reference);
    explicit JobPromiseType(
        std::unique_ptr<script::ValuePromiseWrappable::Reference>
            promise_reference);

    template <typename PromiseReference>
    static std::unique_ptr<JobPromiseType> Create(
        PromiseReference promise_reference) {
      return std::unique_ptr<JobPromiseType>(
          new JobPromiseType(std::move(promise_reference)));
    }

    void Resolve(const bool result);
    void Resolve(const scoped_refptr<ServiceWorkerRegistration>& result);
    void Reject(script::SimpleExceptionType exception);
    void Reject(const scoped_refptr<script::ScriptException>& result);

    script::PromiseState State();

   private:
    std::unique_ptr<script::ValuePromiseBool::Reference>
        promise_bool_reference_;
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_wrappable_reference_;
  };

  // https://w3c.github.io/ServiceWorker/#dfn-job
  struct Job {
    Job(JobType type, const url::Origin& storage_key, const GURL& scope_url,
        const GURL& script_url, std::unique_ptr<JobPromiseType> promise,
        web::EnvironmentSettings* client)
        : type(type),
          storage_key(storage_key),
          scope_url(scope_url),
          script_url(script_url),
          promise(std::move(promise)),
          client(client) {}
    ~Job() {
      client = nullptr;
      containing_job_queue = nullptr;
    }
    JobType type;
    url::Origin storage_key;
    GURL scope_url;
    GURL script_url;
    std::unique_ptr<JobPromiseType> promise;
    web::EnvironmentSettings* client;
    ServiceWorkerUpdateViaCache update_via_cache;
    JobQueue* containing_job_queue = nullptr;
    std::deque<std::unique_ptr<Job>> equivalent_jobs;
    GURL referrer;

    // This lock is for the list of equivalent jobs. It should also be held when
    // resolving the promise.
    base::Lock equivalent_jobs_promise_mutex;
  };

  // https://w3c.github.io/ServiceWorker/#dfn-job-queue
  class JobQueue {
   public:
    bool empty() {
      base::AutoLock lock(mutex_);
      return jobs_.empty();
    }
    void Enqueue(std::unique_ptr<Job> job) {
      base::AutoLock lock(mutex_);
      jobs_.push(std::move(job));
    }
    std::unique_ptr<Job> Dequeue() {
      base::AutoLock lock(mutex_);
      std::unique_ptr<Job> job;
      job.swap(jobs_.front());
      jobs_.pop();
      return job;
    }
    Job* FirstItem() {
      base::AutoLock lock(mutex_);
      return jobs_.empty() ? nullptr : jobs_.front().get();
    }

    // Also return a held autolock, to ensure the item remains a valid item in
    // the queue while it's in use.
    std::pair<Job*, base::sequence_manager::MoveableAutoLock> LastItem() {
      base::sequence_manager::MoveableAutoLock lock(mutex_);
      Job* job = jobs_.empty() ? nullptr : jobs_.back().get();
      return std::pair<Job*, base::sequence_manager::MoveableAutoLock>(
          job, std::move(lock));
    }

   private:
    base::Lock mutex_;
    std::queue<std::unique_ptr<Job>> jobs_;
  };

  // https://w3c.github.io/ServiceWorker/#dfn-scope-to-job-queue-map
  using JobQueueMap = std::map<std::string, std::unique_ptr<JobQueue>>;

  // Type to hold the errorData for rejection of promises.
  class PromiseErrorData {
   public:
    explicit PromiseErrorData(const script::MessageType& message_type)
        : message_type_(message_type),
          exception_code_(dom::DOMException::kNone) {}
    PromiseErrorData(const dom::DOMException::ExceptionCode& code,
                     const std::string& message)
        : message_type_(script::kNoError),
          exception_code_(code),
          message_(message) {}

    void Reject(std::unique_ptr<JobPromiseType> promise) const;

   private:
    // Use script::MessageType because it can hold kNoError value to distinguish
    // between simple exceptions and DOM exceptions.
    script::MessageType message_type_;
    const dom::DOMException::ExceptionCode exception_code_;
    const std::string message_;
  };


  // https://w3c.github.io/ServiceWorker/#create-job
  std::unique_ptr<Job> CreateJob(JobType type, const url::Origin& storage_key,
                                 const GURL& scope_url, const GURL& script_url,
                                 std::unique_ptr<JobPromiseType> promise,
                                 web::EnvironmentSettings* client);

  // https://w3c.github.io/ServiceWorker/#schedule-job
  void ScheduleJob(std::unique_ptr<Job> job);

  // https://w3c.github.io/ServiceWorker/#dfn-job-equivalent
  bool EquivalentJobs(Job* one, Job* two);

  // https://w3c.github.io/ServiceWorker/#run-job-algorithm
  void RunJob(JobQueue* job_queue);

  // Task for "Run Job" to run in the service worker thread.
  void RunJobTask(JobQueue* job_queue);

  // https://w3c.github.io/ServiceWorker/#register-algorithm
  void Register(Job* job);

  // https://w3c.github.io/ServiceWorker/#update-algorithm
  void Update(Job* job);

  // https://w3c.github.io/ServiceWorker/#unregister-algorithm
  void Unregister(Job* job);

  // https://w3c.github.io/ServiceWorker/#reject-job-promise
  void RejectJobPromise(Job* job, const PromiseErrorData& error_data);

  // https://w3c.github.io/ServiceWorker/#resolve-job-promise-algorithm
  void ResolveJobPromise(Job* job, ServiceWorkerRegistrationObject* value);
  void ResolveJobPromiseTask(JobType type,
                             std::unique_ptr<JobPromiseType> promise,
                             web::EnvironmentSettings* client,
                             ServiceWorkerRegistrationObject* value);

  // https://w3c.github.io/ServiceWorker/#finish-job-algorithm
  void FinishJob(Job* job);

  // https://w3c.github.io/ServiceWorker/#get-registration-algorithm
  ServiceWorkerRegistrationObject* GetRegistration(
      const url::Origin& storage_key, const GURL& scope);

  // https://w3c.github.io/ServiceWorker/#set-registration-algorithm
  ServiceWorkerRegistrationObject* SetRegistration(
      const url::Origin& storage_key, const GURL& scope,
      const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://w3c.github.io/ServiceWorker/#get-newest-worker
  ServiceWorker* GetNewestWorker(ServiceWorkerRegistrationObject* registration);

  network::NetworkModule* network_module_;
  base::MessageLoop* message_loop_;

  JobQueueMap job_queue_map_;

  // A registration map is an ordered map where the keys are (storage key,
  // serialized scope urls) and the values are service worker registrations.
  //   https://w3c.github.io/ServiceWorker/#dfn-scope-to-registration-map
  using RegistrationKey = std::pair<url::Origin, std::string>;
  std::map<RegistrationKey, std::unique_ptr<ServiceWorkerRegistrationObject>>
      registration_map_;

  // This lock is to allow atomic operations on the registration map.
  base::Lock registration_map_mutex_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_JOBS_H_
