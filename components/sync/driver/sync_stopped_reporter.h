// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_STOPPED_REPORTER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_STOPPED_REPORTER_H_

#include <memory>
#include <string>

#include "base/timer/timer.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace syncer {

// Manages informing the sync server that sync has been disabled.
class SyncStoppedReporter {
 public:
  SyncStoppedReporter(
      const GURL& sync_service_url,
      const std::string& user_agent,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  SyncStoppedReporter(const SyncStoppedReporter&) = delete;
  SyncStoppedReporter& operator=(const SyncStoppedReporter&) = delete;

  ~SyncStoppedReporter();

  // Inform the sync server that sync was stopped on this device.
  // |access_token|, |cache_guid|, and |birthday| must not be empty.
  void ReportSyncStopped(const std::string& access_token,
                         const std::string& cache_guid,
                         const std::string& birthday);

  // Convert the base sync URL into the sync event URL.
  // Public so tests can use it.
  static GURL GetSyncEventURL(const GURL& sync_service_url);

 private:
  // Callback for a request timing out.
  void OnTimeout();

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Handles timing out requests.
  base::OneShotTimer timer_;

  // The URL for the sync server's event RPC.
  const GURL sync_event_url_;

  // The user agent for the browser.
  const std::string user_agent_;

  // The URL loader for the network request.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current URL loader. Null unless a request is in progress.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_STOPPED_REPORTER_H_
