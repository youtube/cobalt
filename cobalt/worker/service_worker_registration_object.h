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

#ifndef COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_
#define COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "cobalt/worker/service_worker_object.h"
#include "cobalt/worker/service_worker_update_via_cache.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace worker {

// This class represents the 'service worker registration'.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#dfn-service-worker-registration
// Not to be confused with the ServiceWorkerRegistration JavaScript object,
// this represents the registration of the service worker in the browser,
// independent from the JavaScript realm. The lifetime of this object is beyond
// that of the ServiceWorkerRegistration JavaScript object(s) that represent
// this object in their service worker clients.
//   https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-registration-lifetime
class ServiceWorkerRegistrationObject
    : public base::RefCountedThreadSafe<ServiceWorkerRegistrationObject> {
 public:
  ServiceWorkerRegistrationObject(
      const url::Origin& storage_key, const GURL& scope_url,
      const ServiceWorkerUpdateViaCache& update_via_cache_mode);
  ~ServiceWorkerRegistrationObject();

  void AbortAll();

  const url::Origin& storage_key() const { return storage_key_; }

  const GURL& scope_url() const { return scope_url_; }

  void set_update_via_cache_mode(
      const ServiceWorkerUpdateViaCache& update_via_cache_mode) {
    update_via_cache_mode_ = update_via_cache_mode;
  }

  const ServiceWorkerUpdateViaCache& update_via_cache_mode() const {
    return update_via_cache_mode_;
  }

  void set_installing_worker(const scoped_refptr<ServiceWorkerObject>& worker) {
    installing_worker_ = worker;
  }
  const scoped_refptr<ServiceWorkerObject>& installing_worker() const {
    return installing_worker_;
  }
  void set_waiting_worker(const scoped_refptr<ServiceWorkerObject>& worker) {
    waiting_worker_ = worker;
  }
  const scoped_refptr<ServiceWorkerObject>& waiting_worker() const {
    return waiting_worker_;
  }
  void set_active_worker(const scoped_refptr<ServiceWorkerObject>& worker) {
    active_worker_ = worker;
  }
  const scoped_refptr<ServiceWorkerObject>& active_worker() const {
    return active_worker_;
  }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#service-worker-registration-stale
  bool stale() {
    return !last_update_check_time_.is_null() &&
           (base::Time::Now() - last_update_check_time_).InSeconds() >
               kStaleServiceWorkerRegistrationTimeout;
  }

  base::Time last_update_check_time() { return last_update_check_time_; }

  void set_last_update_check_time(base::Time time) {
    last_update_check_time_ = time;
  }

  // https://www.w3.org/TR/2022/CRD-service-workers-20220712/#get-newest-worker
  scoped_refptr<ServiceWorkerObject> GetNewestWorker();

  const int kStaleServiceWorkerRegistrationTimeout = 86400;

 private:
  // This lock is to allow atomic operations on the registration object.
  base::Lock mutex_;

  url::Origin storage_key_;
  GURL scope_url_;
  ServiceWorkerUpdateViaCache update_via_cache_mode_;
  scoped_refptr<ServiceWorkerObject> installing_worker_;
  scoped_refptr<ServiceWorkerObject> waiting_worker_;
  scoped_refptr<ServiceWorkerObject> active_worker_;

  base::Time last_update_check_time_;
};

}  // namespace worker
}  // namespace cobalt

#endif  // COBALT_WORKER_SERVICE_WORKER_REGISTRATION_OBJECT_H_
