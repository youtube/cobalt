// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_
#define COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace metrics {
class MetricsServiceClient;
class MetricsStateManager;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace variations {
class VariationsService;
}

namespace metrics_services_manager {

// MetricsServicesManagerClient is an interface that allows
// MetricsServicesManager to interact with its embedder.
class MetricsServicesManagerClient {
 public:
  virtual ~MetricsServicesManagerClient() {}

  // Methods that create the various services in the context of the embedder.
  virtual std::unique_ptr<variations::VariationsService>
  CreateVariationsService() = 0;
  virtual std::unique_ptr<metrics::MetricsServiceClient>
  CreateMetricsServiceClient() = 0;

  // Gets the MetricsStateManager, creating it if it has not already been
  // created.
  virtual metrics::MetricsStateManager* GetMetricsStateManager() = 0;

  // Returns the URL loader factory which the metrics services should use.
  virtual scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactory() = 0;

  // Returns whether metrics reporting is enabled.
  virtual bool IsMetricsReportingEnabled() = 0;

  // Returns whether metrics consent is given.
  virtual bool IsMetricsConsentGiven() = 0;

  // Returns whether there are any OffTheRecord browsers/tabs open.
  virtual bool IsOffTheRecordSessionActive() = 0;

  // Update the running state of metrics services managed by the embedder, for
  // example, crash reporting.
  virtual void UpdateRunningServices(bool may_record, bool may_upload) {}
};

}  // namespace metrics_services_manager

#endif  // COMPONENTS_METRICS_SERVICES_MANAGER_METRICS_SERVICES_MANAGER_CLIENT_H_
