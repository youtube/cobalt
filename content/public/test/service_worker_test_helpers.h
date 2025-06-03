// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_
#define CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_

#include "base/functional/callback_forward.h"
#include "base/test/simple_test_tick_clock.h"

class GURL;

namespace blink {
struct PlatformNotificationData;
}  // namespace blink

namespace content {

class ServiceWorkerContext;

// Stops the active service worker of the registration for the given |scope|,
// and calls |complete_callback_ui| callback on UI thread when done.
//
// Can be called from UI/IO thread.
void StopServiceWorkerForScope(ServiceWorkerContext* context,
                               const GURL& scope,
                               base::OnceClosure complete_callback_ui);

// Dispatches a notification click event to the active service worker
// worker for the given |scope|.
void DispatchServiceWorkerNotificationClick(
    ServiceWorkerContext* context,
    const GURL& scope,
    const blink::PlatformNotificationData& notification_data);

// Advance the clock of a service worker after the timeout for requests.
void AdvanceClockAfterRequestTimeout(ServiceWorkerContext* context,
                                     int64_t service_worker_version_id,
                                     base::SimpleTestTickClock* tick_clock);

// Tests that uses AdvanceClockAfterRequestTimeout() should call this before
// `tick_clock` is destroyed. Otherwise, `tick_clock` pointer will become
// dangling.
void ResetTickClockToDefaultForAllLiveServiceWorkerVersions(
    ServiceWorkerContext* context);

// Runs the user tasks on a service worker, triggers a timeout and returns
// whether the service worker is still running.
bool TriggerTimeoutAndCheckRunningState(ServiceWorkerContext* context,
                                        int64_t service_worker_version_id);

// Returns whether the worker appears to be in each blink::EmbeddedWorkerStatus.
bool CheckServiceWorkerIsRunning(ServiceWorkerContext* context,
                                 int64_t service_worker_version_id);
bool CheckServiceWorkerIsStarting(ServiceWorkerContext* context,
                                  int64_t service_worker_version_id);
bool CheckServiceWorkerIsStopping(ServiceWorkerContext* context,
                                  int64_t service_worker_version_id);
bool CheckServiceWorkerIsStopped(ServiceWorkerContext* context,
                                 int64_t service_worker_version_id);

void SetServiceWorkerIdleDelay(ServiceWorkerContext* context,
                               int64_t service_worker_version_id,
                               base::TimeDelta delay);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SERVICE_WORKER_TEST_HELPERS_H_
