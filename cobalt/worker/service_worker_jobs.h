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

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>

#include "base/synchronization/lock.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/web/environment_settings.h"
#include "cobalt/worker/service_worker.h"
#include "cobalt/worker/service_worker_registration.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "cobalt/worker/worker_type.h"
#include "url/gurl.h"

namespace cobalt {
namespace worker {

// Algorithms for Service Worker Jobs.
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
                     const script::Handle<script::Promise<void>>& promise,
                     web::EnvironmentSettings* client, const WorkerType& type,
                     const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://w3c.github.io/ServiceWorker/#scope-match-algorithm
  void MatchServiceWorkerRegistration(const GURL& client_url);

 private:
  // https://w3c.github.io/ServiceWorker/#dfn-job-type
  enum JobType { kRegister, kUpdate, kUnregister };

  class JobQueue;

  // https://w3c.github.io/ServiceWorker/#dfn-job
  struct Job {
    Job(JobType type, const GURL& scope_url, const GURL& script_url,
        const script::Handle<script::Promise<void>>& promise,
        web::EnvironmentSettings* client)
        : type(type),
          scope_url(scope_url),
          script_url(script_url),
          promise(promise),
          client(client) {}
    JobType type;
    const GURL& scope_url;
    const GURL& script_url;
    const script::Handle<script::Promise<void>>& promise;
    web::EnvironmentSettings* client;
    ServiceWorkerUpdateViaCache update_via_cache;
    JobQueue* containing_job_queue = nullptr;
    std::queue<std::unique_ptr<Job>> equivalent_jobs;
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
      std::unique_ptr<Job> job(std::move(jobs_.front()));
      jobs_.pop();
      return job;
    }
    Job* FirstItem() {
      base::AutoLock lock(mutex_);
      return jobs_.empty() ? nullptr : jobs_.front().get();
    }
    Job* LastItem() {
      base::AutoLock lock(mutex_);
      return jobs_.empty() ? nullptr : jobs_.back().get();
    }

   private:
    base::Lock mutex_;
    std::queue<std::unique_ptr<Job>> jobs_;
  };

  // https://w3c.github.io/ServiceWorker/#dfn-scope-to-job-queue-map
  typedef std::map<std::string, std::unique_ptr<JobQueue>> JobQueueMap;

  // https://w3c.github.io/ServiceWorker/#create-job
  std::unique_ptr<Job> CreateJob(
      JobType type, const GURL& scope_url, const GURL& script_url,
      const script::Handle<script::Promise<void>>& promise,
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
  void RejectJobPromise(Job* job, const script::ScriptException* error_data);

  // https://w3c.github.io/ServiceWorker/#resolve-job-promise-algorithm
  void ResolveJobPromise(Job* job, ServiceWorkerRegistration* registration);

  // https://w3c.github.io/ServiceWorker/#finish-job-algorithm
  void FinishJob(Job* job);

  // https://w3c.github.io/ServiceWorker/#get-registration-algorithm
  ServiceWorkerRegistration* GetRegistration(const GURL& url);

  // https://w3c.github.io/ServiceWorker/#set-registration-algorithm
  ServiceWorkerRegistration* SetRegistration(
      const GURL& scope_url,
      const ServiceWorkerUpdateViaCache& update_via_cache);

  // https://w3c.github.io/ServiceWorker/#get-newest-worker
  ServiceWorker* GetNewestWorker(ServiceWorkerRegistration* registration);

  network::NetworkModule* network_module_;
  base::MessageLoop* message_loop_;

  JobQueueMap job_queue_map_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_JOBS_H_
