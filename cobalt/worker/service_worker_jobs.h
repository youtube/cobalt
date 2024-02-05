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
#include <string>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_loop.h"
#include "base/optional.h"
#include "base/synchronization/lock.h"
#include "base/task/common/checked_lock.h"
#include "cobalt/loader/fetcher_factory.h"
#include "cobalt/loader/script_loader_factory.h"
#include "cobalt/network/network_module.h"
#include "cobalt/script/exception_message.h"
#include "cobalt/script/promise.h"
#include "cobalt/script/script_value.h"
#include "cobalt/script/script_value_factory.h"
#include "cobalt/web/context.h"
#include "cobalt/web/dom_exception.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_registration_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "starboard/common/atomic.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

class ServiceWorkerContext;

// Algorithms for Service Worker Jobs.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#algorithms
class ServiceWorkerJobs {
 public:
  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-job-type
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
    void Resolve(const scoped_refptr<cobalt::script::Wrappable>& result);
    void Reject(script::SimpleExceptionType exception);
    void Reject(web::DOMException::ExceptionCode code,
                const std::string& message);
    void Reject(const scoped_refptr<script::ScriptException>& result);

    bool is_pending() const { return is_pending_.load(); }

   private:
    starboard::atomic_bool is_pending_{true};
    std::unique_ptr<script::ValuePromiseBool::Reference>
        promise_bool_reference_;
    std::unique_ptr<script::ValuePromiseWrappable::Reference>
        promise_wrappable_reference_;
  };

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-job
  struct Job {
    Job(JobType type, const url::Origin& storage_key, const GURL& scope_url,
        const GURL& script_url, web::Context* client,
        std::unique_ptr<JobPromiseType> promise)
        : type(type),
          storage_key(storage_key),
          scope_url(scope_url),
          script_url(script_url),
          update_via_cache(
              ServiceWorkerUpdateViaCache::kServiceWorkerUpdateViaCacheImports),
          client(client),
          promise(std::move(promise)) {}
    ~Job() {
      client = nullptr;
      containing_job_queue = nullptr;
    }

    // Job properties from the spec.
    //
    JobType type;
    url::Origin storage_key;
    GURL scope_url;
    GURL script_url;
    ServiceWorkerUpdateViaCache update_via_cache;
    web::Context* client;
    GURL referrer;
    std::unique_ptr<JobPromiseType> promise;
    JobQueue* containing_job_queue = nullptr;
    std::deque<std::unique_ptr<Job>> equivalent_jobs;
    bool force_bypass_cache_flag = false;
    bool no_promise_okay = false;

    // Custom, not in the spec.
    //

    // This lock is for the list of equivalent jobs. It should also be held when
    // resolving the promise.
    base::Lock equivalent_jobs_promise_mutex;

    // The loader that is used for asynchronous loads.
    std::unique_ptr<loader::Loader> loader;
  };

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-job-queue
  class JobQueue {
   public:
    bool empty() {
      base::AutoLock lock(mutex_);
      return jobs_.empty();
    }
    void Enqueue(std::unique_ptr<Job> job) {
      base::AutoLock lock(mutex_);
      jobs_.push_back(std::move(job));
    }
    std::unique_ptr<Job> Dequeue() {
      base::AutoLock lock(mutex_);
      std::unique_ptr<Job> job;
      job.swap(jobs_.front());
      jobs_.pop_front();
      return job;
    }
    Job* FirstItem() {
      base::AutoLock lock(mutex_);
      return jobs_.empty() ? nullptr : jobs_.front().get();
    }

    // Also return a held autolock, to ensure the item remains a valid item in
    // the queue while it's in use.
    std::pair<Job*, base::AutoLock> LastItem() {
      base::AutoLock lock(mutex_);
      Job* job = jobs_.empty() ? nullptr : jobs_.back().get();
      return std::pair<Job*, base::AutoLock>(job, std::move(lock));
    }

    // Ensure no references are kept to JS objects for a client that is about to
    // be shutdown.
    void PrepareForClientShutdown(web::Context* client);

    // Helper method for PrepareForClientShutdown to help with recursion to
    // equivalent jobs.
    void PrepareJobForClientShutdown(const std::unique_ptr<Job>& job,
                                     web::Context* client);

   private:
    base::Lock mutex_;
    std::deque<std::unique_ptr<Job>> jobs_;
  };

  ServiceWorkerJobs(ServiceWorkerContext* service_worker_context,
                    network::NetworkModule* network_module,
                    base::MessageLoop* message_loop);
  ~ServiceWorkerJobs();

  base::MessageLoop* message_loop() { return message_loop_; }

  // Ensure no references are kept to JS objects for a client that is about to
  // be shutdown.
  void PrepareForClientShutdown(web::Context* client);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#create-job
  std::unique_ptr<Job> CreateJob(
      JobType type, const url::Origin& storage_key, const GURL& scope_url,
      const GURL& script_url,
      std::unique_ptr<script::ValuePromiseWrappable::Reference> promise,
      web::Context* client) {
    return CreateJob(type, storage_key, scope_url, script_url,
                     JobPromiseType::Create(std::move(promise)), client);
  }
  std::unique_ptr<Job> CreateJob(
      JobType type, const url::Origin& storage_key, const GURL& scope_url,
      const GURL& script_url,
      std::unique_ptr<script::ValuePromiseBool::Reference> promise,
      web::Context* client) {
    return CreateJob(type, storage_key, scope_url, script_url,
                     JobPromiseType::Create(std::move(promise)), client);
  }
  std::unique_ptr<Job> CreateJobWithoutPromise(JobType type,
                                               const url::Origin& storage_key,
                                               const GURL& scope_url,
                                               const GURL& script_url) {
    auto job = CreateJob(type, storage_key, scope_url, script_url,
                         std::unique_ptr<JobPromiseType>(), /*client=*/nullptr);
    job->no_promise_okay = true;
    return job;
  }
  std::unique_ptr<Job> CreateJob(
      JobType type, const url::Origin& storage_key, const GURL& scope_url,
      const GURL& script_url, std::unique_ptr<JobPromiseType> promise = nullptr,
      web::Context* client = nullptr);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#schedule-job
  void ScheduleJob(std::unique_ptr<Job> job);

 private:
  friend class ServiceWorkerContext;

  // State used for the 'Update' algorithm.
  struct UpdateJobState : public base::RefCounted<UpdateJobState> {
    UpdateJobState(
        Job* job,
        const scoped_refptr<ServiceWorkerRegistrationObject>& registration,
        ServiceWorkerObject* newest_worker)
        : job(job), registration(registration), newest_worker(newest_worker) {}
    Job* job;
    scoped_refptr<ServiceWorkerRegistrationObject> registration;
    ServiceWorkerObject* newest_worker;

    // Headers received with the main service worker script load.
    scoped_refptr<net::HttpResponseHeaders> script_headers;

    // map of content or resources for the worker.
    ScriptResourceMap updated_resource_map;

    // This represents hasUpdatedResources of the Update algorithm.
    // True if any of the resources has changed since last cached.
    bool has_updated_resources = false;
  };

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-scope-to-job-queue-map
  using JobQueueMap = std::map<std::string, std::unique_ptr<JobQueue>>;

  // Type to hold the errorData for rejection of promises.
  class PromiseErrorData {
   public:
    explicit PromiseErrorData(const script::MessageType& message_type)
        : message_type_(message_type),
          exception_code_(web::DOMException::kNone) {}
    PromiseErrorData(const web::DOMException::ExceptionCode& code,
                     const std::string& message)
        : message_type_(script::kNoError),
          exception_code_(code),
          message_(message) {}

    void Reject(std::unique_ptr<JobPromiseType> promise) const;

   private:
    // Use script::MessageType because it can hold kNoError value to distinguish
    // between simple exceptions and DOM exceptions.
    script::MessageType message_type_;
    const web::DOMException::ExceptionCode exception_code_;
    const std::string message_;
  };

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-job-equivalent
  bool ReturnJobsAreEquivalent(Job* one, Job* two);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#run-job-algorithm
  void RunJob(JobQueue* job_queue);

  // Task for "Run Job" to run in the service worker thread.
  void RunJobTask(JobQueue* job_queue);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#register-algorithm
  void Register(Job* job);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#update-algorithm
  void Update(Job* job);

  void UpdateOnContentProduced(scoped_refptr<UpdateJobState> state,
                               const loader::Origin& last_url_origin,
                               std::unique_ptr<std::string> content);
  bool UpdateOnResponseStarted(
      scoped_refptr<UpdateJobState> state, loader::Fetcher* fetcher,
      const scoped_refptr<net::HttpResponseHeaders>& headers);
  void UpdateOnLoadingComplete(scoped_refptr<UpdateJobState> state,
                               const base::Optional<std::string>& error);

  void UpdateOnRunServiceWorker(scoped_refptr<UpdateJobState> state,
                                scoped_refptr<ServiceWorkerObject> worker,
                                bool run_result);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#unregister-algorithm
  void Unregister(Job* job);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#reject-job-promise
  void RejectJobPromise(Job* job, const PromiseErrorData& error_data);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#resolve-job-promise-algorithm
  void ResolveJobPromise(
      Job* job, const scoped_refptr<ServiceWorkerRegistrationObject>& value) {
    ResolveJobPromise(job, false, value);
  }
  void ResolveJobPromise(Job* job, bool value,
                         const scoped_refptr<ServiceWorkerRegistrationObject>&
                             registration = nullptr);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#finish-job-algorithm
  void FinishJob(Job* job);

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#installation-algorithm
  void Install(
      Job* job, const scoped_refptr<ServiceWorkerObject>& worker,
      const scoped_refptr<ServiceWorkerRegistrationObject>& registration);

  ServiceWorkerContext* service_worker_context_;

  // FetcherFactory that is used to create a fetcher according to URL.
  std::unique_ptr<loader::FetcherFactory> fetcher_factory_;
  // LoaderFactory that is used to acquire references to resources from a URL.
  std::unique_ptr<loader::ScriptLoaderFactory> script_loader_factory_;

  base::MessageLoop* message_loop_;

  JobQueueMap job_queue_map_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_JOBS_H_
