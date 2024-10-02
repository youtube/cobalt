// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_SERVICE_WORKER_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_SERVICE_WORKER_REGISTRATION_WAITER_H_

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}  // namespace content

namespace web_app {

class ServiceWorkerRegistrationWaiter
    : public content::ServiceWorkerContextObserver {
 public:
  ServiceWorkerRegistrationWaiter(content::BrowserContext* browser_context,
                                  const GURL& url);
  ServiceWorkerRegistrationWaiter(content::StoragePartition* storage_partition,
                                  const GURL& url);
  ServiceWorkerRegistrationWaiter(const ServiceWorkerRegistrationWaiter&) =
      delete;
  ServiceWorkerRegistrationWaiter& operator=(
      const ServiceWorkerRegistrationWaiter&) = delete;
  ~ServiceWorkerRegistrationWaiter() override;

  void AwaitRegistration(
      const base::Location& location = base::Location::Current());

 private:
  // content::ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& pattern) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

  raw_ptr<content::ServiceWorkerContext> service_worker_context_;
  const GURL url_;
  base::RunLoop run_loop_;

};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_SERVICE_WORKER_REGISTRATION_WAITER_H_
